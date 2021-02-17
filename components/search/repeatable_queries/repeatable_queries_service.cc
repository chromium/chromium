// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/repeatable_queries/repeatable_queries_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/stl_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/url_database.h"
#include "components/search/ntp_features.h"
#include "components/search/search_provider_observer.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {
const char kXSSIResponsePreamble[] = ")]}'";
const size_t kMaxQueries = 2;

bool JsonToRepeatableQueriesData(const base::Value& root_value,
                                 std::vector<RepeatableQuery>* data) {
  // 1st element is the query. 2nd element is the list of results.
  base::string16 query;
  const base::ListValue* root_list = nullptr;
  const base::ListValue* results_list = nullptr;
  if (!root_value.GetAsList(&root_list) || !root_list->GetString(0, &query) ||
      !query.empty() || !root_list->GetList(1, &results_list))
    return false;

  // Ignore the 3rd and 4th elements. 5th element is the key-value pairs from
  // the Suggest server containing the deletion URLs.
  const base::DictionaryValue* extras = nullptr;
  const base::ListValue* suggestion_details = nullptr;
  if (!root_list->GetDictionary(4, &extras) ||
      !extras->GetList("google:suggestdetail", &suggestion_details) ||
      suggestion_details->GetSize() != results_list->GetSize()) {
    return false;
  }

  base::string16 suggestion;
  for (size_t index = 0; results_list->GetString(index, &suggestion); ++index) {
    RepeatableQuery result;
    result.query = base::CollapseWhitespace(suggestion, false);
    if (result.query.empty())
      continue;

    const base::DictionaryValue* suggestion_detail = nullptr;
    if (suggestion_details->GetDictionary(index, &suggestion_detail)) {
      suggestion_detail->GetString("du", &result.deletion_url);
    }
    data->push_back(result);
  }

  return !data->empty();
}
}  // namespace

// static
const char RepeatableQueriesService::kExtractedCountHistogram[] =
    "NewTabPage.RepeatableQueries.ExtractedCount";
const char RepeatableQueriesService::kExtractionDurationHistogram[] =
    "NewTabPage.RepeatableQueries.ExtractionDuration";

class RepeatableQueriesService::SigninObserver
    : public signin::IdentityManager::Observer {
 public:
  SigninObserver(signin::IdentityManager* identity_manager,
                 base::RepeatingClosure callback)
      : identity_manager_(identity_manager), callback_(std::move(callback)) {
    if (identity_manager_) {
      identity_manager_observation_.Observe(identity_manager_);
    }
  }
  ~SigninObserver() override = default;

  bool IsSignedIn() {
    return identity_manager_ ? !identity_manager_->GetAccountsInCookieJar()
                                    .signed_in_accounts.empty()
                             : false;
  }

 private:
  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override {
    callback_.Run();
  }

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  // May be nullptr in tests.
  signin::IdentityManager* const identity_manager_;
  base::RepeatingClosure callback_;
};

RepeatableQueriesService::RepeatableQueriesService(
    signin::IdentityManager* identity_manager,
    history::HistoryService* history_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& request_initiator_url)
    : history_service_(history_service),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory),
      request_initiator_url_(request_initiator_url),
      signin_observer_(std::make_unique<SigninObserver>(
          identity_manager,
          base::BindRepeating(&RepeatableQueriesService::SigninStatusChanged,
                              base::Unretained(this)))),
      search_provider_observer_(std::make_unique<SearchProviderObserver>(
          template_url_service,
          base::BindRepeating(&RepeatableQueriesService::SearchProviderChanged,
                              base::Unretained(this)))),
      deletion_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {
  DCHECK(history_service_);
  DCHECK(template_url_service_);
  DCHECK(url_loader_factory_);
}

RepeatableQueriesService::~RepeatableQueriesService() = default;

void RepeatableQueriesService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnRepeatableQueriesServiceShuttingDown();
  }
}

const std::vector<RepeatableQuery>&
RepeatableQueriesService::repeatable_queries() const {
  return repeatable_queries_;
}

void RepeatableQueriesService::Refresh() {
  if (!search_provider_observer()->is_google()) {
    NotifyObservers();
    return;
  }

  if (signin_observer()->IsSignedIn()) {
    GetRepeatableQueriesFromServer();
  } else {
    GetRepeatableQueriesFromURLDatabase();
  }
}

void RepeatableQueriesService::DeleteQueryWithDestinationURL(const GURL& url) {
  auto it = std::find_if(repeatable_queries_.begin(), repeatable_queries_.end(),
                         [&url](const auto& repeatable_query) {
                           return repeatable_query.destination_url == url;
                         });

  // Return if no repeatable query with a matching destination URL exists.
  if (it == repeatable_queries_.end()) {
    // Still notify observers of the deletion attempt.
    NotifyObservers();
    return;
  }

  if (it->deletion_url.empty()) {
    DeleteRepeatableQueryFromURLDatabase(it->query);
  } else {
    DeleteRepeatableQueryFromServer(it->deletion_url);
  }

  // Delete all the Google search URLs for the given query from history.
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (default_provider) {
    history_service_->DeleteMatchingURLsForKeyword(default_provider->id(),
                                                   it->query);
  }

  // Make sure the query is not suggested again.
  MarkQueryAsDeleted(it->query);

  // Update the repeatable queries and notify the observers.
  repeatable_queries_.erase(it);
  NotifyObservers();
}

void RepeatableQueriesService::AddObserver(
    RepeatableQueriesServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void RepeatableQueriesService::RemoveObserver(
    RepeatableQueriesServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

RepeatableQueriesService::SigninObserver*
RepeatableQueriesService::signin_observer() {
  return signin_observer_.get();
}

SearchProviderObserver* RepeatableQueriesService::search_provider_observer() {
  return search_provider_observer_.get();
}

void RepeatableQueriesService::SearchProviderChanged() {
  // If we have cached data, clear it.
  repeatable_queries_.clear();
  Refresh();
}

void RepeatableQueriesService::SigninStatusChanged() {
  // If we have cached data, clear it.
  repeatable_queries_.clear();
  Refresh();
}

GURL RepeatableQueriesService::GetQueryDestinationURL(
    const base::string16& query,
    const TemplateURL* search_provider) {
  DCHECK(search_provider);

  TemplateURLRef::SearchTermsArgs search_terms_args(query);
  const TemplateURLRef& search_url_ref = search_provider->url_ref();
  const SearchTermsData& search_terms_data =
      template_url_service_->search_terms_data();
  DCHECK(search_url_ref.SupportsReplacement(search_terms_data));
  return GURL(
      search_url_ref.ReplaceSearchTerms(search_terms_args, search_terms_data));
}

GURL RepeatableQueriesService::GetQueryDeletionURL(
    const std::string& deletion_url) {
  const auto* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (!default_provider)
    return GURL();
  const SearchTermsData& search_terms_data =
      template_url_service_->search_terms_data();
  GURL request_url = default_provider->GenerateSearchURL(search_terms_data);
  return request_url.GetOrigin().Resolve(deletion_url);
}

GURL RepeatableQueriesService::GetRequestURL() {
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.request_source = TemplateURLRef::NON_SEARCHBOX_NTP;
  const TemplateURLRef& suggestion_url_ref =
      template_url_service_->GetDefaultSearchProvider()->suggestions_url_ref();
  const SearchTermsData& search_terms_data =
      template_url_service_->search_terms_data();
  DCHECK(suggestion_url_ref.SupportsReplacement(search_terms_data));
  return GURL(suggestion_url_ref.ReplaceSearchTerms(search_terms_args,
                                                    search_terms_data));
}

void RepeatableQueriesService::FlushForTesting(base::OnceClosure flushed) {
  deletion_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                          std::move(flushed));
}

void RepeatableQueriesService::GetRepeatableQueriesFromServer() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("repeatable_queries_service", R"(
        semantics {
          sender: "Repeatable Queries Service"
          description:
            "Downloads search queries to be shown on the Most Visited "
            "section of New Tab Page to signed-in users based on their "
            "previous search history."
          trigger:
            "Displaying the new tab page, if Google is the "
            "configured search provider, and the user is signed in."
          data: "Google credentials if user is signed in."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature by selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine', or by "
            "signing out of the browser on the New Tab Page. Users can opt "
            "out of this feature by switching to custom shortcuts."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  const GURL& request_url = GetRequestURL();
  variations::AppendVariationsHeaderUnknownSignedIn(
      request_url, variations::InIncognito::kNo, resource_request.get());
  resource_request->url = request_url;
  resource_request->request_initiator =
      url::Origin::Create(request_initiator_url_);

  loaders_.push_back(network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation));
  loaders_.back()->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&RepeatableQueriesService::RepeatableQueriesResponseLoaded,
                     weak_ptr_factory_.GetWeakPtr(), loaders_.back().get()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void RepeatableQueriesService::RepeatableQueriesResponseLoaded(
    network::SimpleURLLoader* loader,
    std::unique_ptr<std::string> response) {
  auto net_error = loader->NetError();
  base::EraseIf(loaders_, [loader](const auto& loader_ptr) {
    return loader == loader_ptr.get();
  });

  if (net_error != net::OK || !response) {
    // In the case of network errors, keep the cached data, if any, but still
    // notify observers of the finished load attempt.
    NotifyObservers();
    return;
  }

  if (base::StartsWith(*response, kXSSIResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    *response = response->substr(strlen(kXSSIResponsePreamble));
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response,
      base::BindOnce(&RepeatableQueriesService::RepeatableQueriesParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RepeatableQueriesService::RepeatableQueriesParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (!default_provider)
    return;

  repeatable_queries_.clear();

  std::vector<RepeatableQuery> queries;
  if (result.value && JsonToRepeatableQueriesData(*result.value, &queries)) {
    for (auto& query : queries) {
      if (IsQueryDeleted(query.query))
        continue;
      query.destination_url =
          GetQueryDestinationURL(query.query, default_provider);
      repeatable_queries_.push_back(query);
      if (repeatable_queries_.size() >= kMaxQueries)
        break;
    }
  }

  NotifyObservers();
}

void RepeatableQueriesService::GetRepeatableQueriesFromURLDatabase() {
  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  if (!default_provider)
    return;

  repeatable_queries_.clear();

  // Fail if the in-memory URLDatabase is not available.
  history::URLDatabase* url_db = history_service_->InMemoryDatabase();
  if (!url_db)
    return;

  const base::TimeTicks db_query_time = base::TimeTicks::Now();
  auto results = url_db->GetMostRecentNormalizedKeywordSearchTerms(
      template_url_service_->GetDefaultSearchProvider()->id(),
      ntp_features::GetLocalHistoryRepeatableQueriesAgeThreshold());

  const base::Time now = base::Time::Now();
  const int kRecencyDecayUnitSec =
      ntp_features::GetLocalHistoryRepeatableQueriesRecencyHalfLifeSeconds();
  const double kFrequencyExponent =
      ntp_features::GetLocalHistoryRepeatableQueriesFrequencyExponent();
  auto CompareByFrecency = [&](const auto& a, const auto& b) {
    return a.GetFrecency(now, kRecencyDecayUnitSec, kFrequencyExponent) >
           b.GetFrecency(now, kRecencyDecayUnitSec, kFrequencyExponent);
  };
  std::sort(results.begin(), results.end(), CompareByFrecency);

  for (const auto& result : results) {
    RepeatableQuery repeatable_query;
    repeatable_query.query = result.normalized_term;
    if (IsQueryDeleted(repeatable_query.query))
      continue;
    repeatable_query.destination_url =
        GetQueryDestinationURL(repeatable_query.query, default_provider);
    repeatable_queries_.push_back(repeatable_query);
    if (repeatable_queries_.size() >= kMaxQueries)
      break;
  }

  base::UmaHistogramTimes(kExtractionDurationHistogram,
                          base::TimeTicks::Now() - db_query_time);
  base::UmaHistogramCounts10000(kExtractedCountHistogram, results.size());

  NotifyObservers();
}

void RepeatableQueriesService::DeleteRepeatableQueryFromServer(
    const std::string& deletion_url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("repeatable_queries_deletion", R"(
        semantics {
          sender: "Repeatable Queries Service"
          description:
            "When users attempt to delete a server-provided repeatable search "
            "query from the Most Visited section of New Tab Page, Chrome sends "
            "a request to the server requesting deletion of that suggestion."
          trigger:
            "User attempts to delete a server-provided repeatable search "
            "query for which the server provided a custom deletion URL from "
            "the Most Visited section of New Tab Page, if Google is the "
            "configured search provider, and the user is signed in."
          data: "Google credentials if user is signed in."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature by selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine', or by "
            "signing out of the browser on the New Tab Page. Users can opt "
            "out of this feature by switching to custom shortcuts."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

  GURL request_url = GetQueryDeletionURL(deletion_url);
  if (!request_url.is_valid())
    return;

  auto deletion_request = std::make_unique<network::ResourceRequest>();
  variations::AppendVariationsHeaderUnknownSignedIn(
      request_url, variations::InIncognito::kNo, deletion_request.get());
  deletion_request->url = request_url;
  deletion_request->request_initiator =
      url::Origin::Create(request_initiator_url_);

  loaders_.push_back(network::SimpleURLLoader::Create(
      std::move(deletion_request), traffic_annotation));
  loaders_.back()->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&RepeatableQueriesService::DeletionResponseLoaded,
                     weak_ptr_factory_.GetWeakPtr(), loaders_.back().get()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void RepeatableQueriesService::DeletionResponseLoaded(
    network::SimpleURLLoader* loader,
    std::unique_ptr<std::string> response) {
  base::EraseIf(loaders_, [loader](const auto& loader_ptr) {
    return loader == loader_ptr.get();
  });
}

void RepeatableQueriesService::DeleteRepeatableQueryFromURLDatabase(
    const base::string16& query) {
  deletion_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RepeatableQueriesService::DeleteRepeatableQueryFromURLDatabaseTask,
          weak_ptr_factory_.GetWeakPtr(), query,
          history_service_->InMemoryDatabase()));
}

void RepeatableQueriesService::DeleteRepeatableQueryFromURLDatabaseTask(
    const base::string16& query,
    history::URLDatabase* url_db) {
  // Fail if the in-memory URLDatabase is not available.
  if (!url_db)
    return;

  // Delete all the search terms matching the repeatable query suggestion from
  // the in-memory URLDatabase.
  url_db->DeleteKeywordSearchTermForNormalizedTerm(
      template_url_service_->GetDefaultSearchProvider()->id(), query);
}

void RepeatableQueriesService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnRepeatableQueriesUpdated();
  }
}

bool RepeatableQueriesService::IsQueryDeleted(const base::string16& query) {
  return base::Contains(deleted_repeatable_queries_, query);
}

void RepeatableQueriesService::MarkQueryAsDeleted(const base::string16& query) {
  deleted_repeatable_queries_.insert(query);
}
