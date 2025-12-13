// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_service_impl.h"

#include <algorithm>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/affiliations/core/browser/affiliation_backend.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace affiliations {

namespace {

void LogFetchResult(GetChangePasswordUrlMetric result) {
  base::UmaHistogramEnumeration(kGetChangePasswordURLMetricName, result);
}

// Creates a look-up (Facet URI : change password URL) map for facets from
// requested |groupings|. If a facet does not have change password URL it gets
// paired with another facet's URL, which belongs to the same group. In case
// none of the group's facets have change password URLs then those facets are
// not inserted to the map.
std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch>
CreateFacetUriToChangePasswordUrlMap(
    const std::vector<GroupedFacets>& groupings) {
  std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch> uri_to_url;
  for (const auto& grouped_facets : groupings) {
    for (const auto& facet : grouped_facets.facets) {
      // Affiliation server didn't generate it. Such facets can be skipped.
      if (facet.is_facet_synthesized) {
        continue;
      }
      uri_to_url[facet.uri] = AffiliationServiceImpl::ChangePasswordUrlMatch{
          .change_password_url = facet.change_password_url};
    }
  }
  return uri_to_url;
}

FacetURI ConvertGURLToFacet(const GURL& url) {
  if (url.SchemeIs(url::kAndroidScheme)) {
    return FacetURI::FromPotentiallyInvalidSpec(url.possibly_invalid_spec());
  } else {
    // Path should be stripped before converting into FacetURI.
    return FacetURI::FromPotentiallyInvalidSpec(url.GetWithEmptyPath().spec());
  }
}

// Returns FacetURI corresponding to the top level domain of the `facet`. Empty
// if `facet` is android app, eTLD+1 can't be extracted, or `facet` is already
// top level domain.
FacetURI GetFacetForTopLevelDomain(
    FacetURI facet,
    const base::flat_set<std::string>& psl_extension_list) {
  if (!facet.IsValidWebFacetURI()) {
    return FacetURI();
  }

  std::string top_domain = GetExtendedTopLevelDomain(
      GURL(facet.canonical_spec()), psl_extension_list);
  if (top_domain.empty()) {
    return FacetURI();
  }

  FacetURI result =
      FacetURI::FromPotentiallyInvalidSpec("https://" + top_domain);

  if (!result.is_valid() || result == facet) {
    return FacetURI();
  }

  return result;
}

void LogChangePasswordURLTypeUsed(
    const AffiliationServiceImpl::ChangePasswordUrlMatch& match) {
  if (match.main_domain_override) {
    LogFetchResult(GetChangePasswordUrlMetric::kMainDomainUsed);
  } else {
    LogFetchResult(GetChangePasswordUrlMetric::kUrlOverrideUsed);
  }
}

}  // namespace

BASE_FEATURE(kCachePSLExtensions, base::FEATURE_ENABLED_BY_DEFAULT);

const char kGetChangePasswordURLMetricName[] =
    "PasswordManager.AffiliationService.GetChangePasswordUsage";

struct AffiliationServiceImpl::FetchInfo {
  FetchInfo(FacetURI facet,
            const base::flat_set<std::string>& psl_extension_list,
            base::OnceClosure result_callback)
      : requested_facet(std::move(facet)),
        top_level_domain(
            GetFacetForTopLevelDomain(requested_facet, psl_extension_list)),
        callback(std::move(result_callback)) {}

  FetchInfo(FetchInfo&& other) = default;

  FetchInfo& operator=(FetchInfo&& other) = default;

  ~FetchInfo() {
    // Check if the callback is still there. |FetchInfo| is moved into a
    // callback, so it can be gone by the time the destructor is called.
    if (callback) {
      // If a fetch is not possible, |AffiliationFetcherManager::Fetch| can
      // invoke its callback immediately. Posting a task here instead or
      // directly running it will prevent the caller of
      // |PrefetchChangePasswordURL| from unexpectedly receiving the result of
      // the callback during the execution of |PrefetchChangePasswordURL|.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback));
    }
  }

  ChangePasswordUrlMatch GetChangePasswordURL(
      const AffiliationFetcherInterface::ParsedFetchResponse& result) const {
    std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch>
        uri_to_url = CreateFacetUriToChangePasswordUrlMap(result.groupings);

    auto it = uri_to_url.find(requested_facet);
    if (it != uri_to_url.end()) {
      return it->second;
    }

    // Check if change password URL available for the main domain.
    it = uri_to_url.find(top_level_domain);
    if (it != uri_to_url.end()) {
      it->second.main_domain_override = true;
      return it->second;
    }

    return ChangePasswordUrlMatch();
  }

  std::vector<FacetURI> FacetsToRequest() const {
    if (top_level_domain.is_valid() && top_level_domain != requested_facet) {
      return {requested_facet, top_level_domain};
    }
    return {requested_facet};
  }

  FacetURI requested_facet;
  FacetURI top_level_domain;
  // Callback is passed in PrefetchChangePasswordURLs and is run to indicate the
  // prefetch has finished or got canceled.
  base::OnceClosure callback;
};

// TODO(crbug.com/40789139): Create the backend task runner in Init and stop
// passing it in the constructor.
AffiliationServiceImpl::AffiliationServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : url_loader_factory_(std::move(url_loader_factory)),
      backend_task_runner_(std::move(backend_task_runner)) {}

AffiliationServiceImpl::~AffiliationServiceImpl() = default;

void AffiliationServiceImpl::Init(
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::FilePath& db_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetcher_manager_ =
      std::make_unique<AffiliationFetcherManager>(url_loader_factory_);
  backend_ = std::make_unique<AffiliationBackend>(
      backend_task_runner_, base::DefaultClock::GetInstance(),
      base::DefaultTickClock::GetInstance());

  PostToBackend(&AffiliationBackend::Initialize, url_loader_factory_->Clone(),
                base::Unretained(network_connection_tracker), db_path);
}

void AffiliationServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_) {
    backend_task_runner_->DeleteSoon(FROM_HERE, std::move(backend_));
  }
}

void AffiliationServiceImpl::PrefetchChangePasswordURL(
    const GURL& url,
    base::OnceClosure callback) {
  FacetURI facet_uri = ConvertGURLToFacet(url);
  if (!facet_uri.is_valid() ||
      base::Contains(change_password_urls_, facet_uri)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  FetchInfo fetch_info(facet_uri, psl_extension_list_, std::move(callback));
  auto facets_to_request = fetch_info.FacetsToRequest();
  fetcher_manager_->Fetch(
      facets_to_request, kChangePasswordUrlRequestInfo,
      base::BindOnce(&AffiliationServiceImpl::OnFetchFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fetch_info)));
}

GURL AffiliationServiceImpl::GetChangePasswordURL(const GURL& url) const {
  FacetURI uri = ConvertGURLToFacet(url);

  auto it = change_password_urls_.find(uri);
  if (it != change_password_urls_.end() &&
      it->second.change_password_url.is_valid()) {
    LogChangePasswordURLTypeUsed(it->second);
    return it->second.change_password_url;
  }
  auto requested_facet_uris = fetcher_manager_->GetRequestedFacetURIs();
  if (base::Contains(requested_facet_uris, uri)) {
    LogFetchResult(GetChangePasswordUrlMetric::kNotFetchedYet);
  } else {
    LogFetchResult(GetChangePasswordUrlMetric::kNoUrlOverrideAvailable);
  }
  return GURL();
}

void AffiliationServiceImpl::OnFetchFinished(
    const FetchInfo& fetch_info,
    AffiliationFetcherInterface::FetchResult fetch_result) {
  // Handle the successful case only. On failure the fetch will be discarded
  // without retries.
  if (fetch_result.IsSuccessful()) {
    change_password_urls_[fetch_info.requested_facet] =
        fetch_info.GetChangePasswordURL(fetch_result.data.value());
  }
}

void AffiliationServiceImpl::GetAffiliationsAndBranding(
    const FacetURI& facet_uri,
    ResultCallback result_callback) {
  PostToBackend(&AffiliationBackend::GetAffiliationsAndBranding, facet_uri,
                std::move(result_callback),
                base::SequencedTaskRunner::GetCurrentDefault());
}

void AffiliationServiceImpl::Prefetch(const FacetURI& facet_uri,
                                      const base::Time& keep_fresh_until) {
  PostToBackend(&AffiliationBackend::Prefetch, facet_uri, keep_fresh_until);
}

void AffiliationServiceImpl::CancelPrefetch(
    const FacetURI& facet_uri,
    const base::Time& keep_fresh_until) {
  PostToBackend(&AffiliationBackend::CancelPrefetch, facet_uri,
                keep_fresh_until);
}

void AffiliationServiceImpl::KeepPrefetchForFacets(
    std::vector<FacetURI> facet_uris) {
  PostToBackend(&AffiliationBackend::KeepPrefetchForFacets,
                std::move(facet_uris));
}

void AffiliationServiceImpl::TrimUnusedCache(std::vector<FacetURI> facet_uris) {
  PostToBackend(&AffiliationBackend::TrimUnusedCache, std::move(facet_uris));
}

void AffiliationServiceImpl::GetGroupingInfo(std::vector<FacetURI> facet_uris,
                                             GroupsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If `backend` is destroyed there is nothing to do.
  if (!backend_) {
    return;
  }

  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::GetGroupingInfo,
                     base::Unretained(backend_.get()), std::move(facet_uris)),
      std::move(callback));
}

void AffiliationServiceImpl::GetPSLExtensions(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If `backend` is destroyed there is nothing to do.
  if (!backend_) {
    return;
  }

  if (psl_extension_list_.empty() &&
      base::FeatureList::IsEnabled(kCachePSLExtensions)) {
    callback =
        base::BindOnce(&AffiliationServiceImpl::OnPSLExtensionsLoaded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  }

  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::GetPSLExtensions,
                     base::Unretained(backend_.get())),
      std::move(callback));
}

void AffiliationServiceImpl::UpdateAffiliationsAndBranding(
    const std::vector<FacetURI>& facets,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  auto callback_in_main_sequence =
      base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                     base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
                     std::move(callback));
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::UpdateAffiliationsAndBranding,
                     base::Unretained(backend_.get()), facets,
                     std::move(callback_in_main_sequence)));
}

void AffiliationServiceImpl::RegisterSource(
    std::unique_ptr<AffiliationSource> source) {
  prefetcher_.RegisterSource(std::move(source));
}

void AffiliationServiceImpl::OnPSLExtensionsLoaded(
    base::OnceCallback<void(std::vector<std::string>)> callback,
    std::vector<std::string> psl_extensions) {
  psl_extension_list_ = base::flat_set<std::string>(psl_extensions);
  std::move(callback).Run(std::move(psl_extensions));
}

}  // namespace affiliations
