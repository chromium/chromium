// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_provider.h"

#include <iterator>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/common/types_util.h"
#include "chrome/browser/ui/app_list/search/omnibox_answer_result.h"
#include "chrome/browser/ui/app_list/search/omnibox_result.h"
#include "chrome/browser/ui/app_list/search/open_tab_result.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;

// Some omnibox answers overtrigger on short queries. This controls the minimum
// query length before they are displayed.
constexpr size_t kMinQueryLengthForCommonAnswers = 4u;

bool IsDriveUrl(const GURL& url) {
  // Returns true if the |url| points to a Drive Web host.
  const std::string& host = url.host();
  return host == "drive.google.com" || host == "docs.google.com";
}

// Returns true if the match is an answer, including calculator answers.
bool IsAnswer(const AutocompleteMatch& match) {
  return match.answer.has_value() ||
         match.type == AutocompleteMatchType::CALCULATOR;
}

// Some answer result types overtrigger on short queries. Returns true if an
// answer should be filtered.
bool ShouldFilterAnswer(const AutocompleteMatch& match,
                        const std::u16string& query) {
  // TODO(crbug.com/1258415): Move this to the filtering ranker once more
  // detailed result subtype info is exposed by ChromeSearchResult.
  if (query.size() >= kMinQueryLengthForCommonAnswers || !match.answer) {
    return false;
  }

  switch (match.answer.value().type()) {
    case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
    case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
      return true;
    default:
      return false;
  }
}

int ProviderTypes() {
  // We use all the default providers except for the document provider, which
  // suggests Drive files on enterprise devices. This is disabled to avoid
  // duplication with search results from DriveFS.
  int providers = AutocompleteClassifier::DefaultOmniboxProviders() &
                  ~AutocompleteProvider::TYPE_DOCUMENT;
  if (ash::features::IsProductivityLauncherEnabled() &&
      base::GetFieldTrialParamByFeatureAsBool(
          ash::features::kProductivityLauncher, "enable_open_tab", true)) {
    providers |= AutocompleteProvider::TYPE_OPEN_TAB;
  }
  return providers;
}

void RemoveDuplicates(std::vector<std::unique_ptr<OmniboxResult>>& results) {
  // Sort the results by deduplication priority and then filter from left to
  // right. This ensures that higher priority results are retained.
  sort(results.begin(), results.end(),
       [](const std::unique_ptr<OmniboxResult>& a,
          const std::unique_ptr<OmniboxResult>& b) {
         return a->dedup_priority() > b->dedup_priority();
       });

  base::flat_set<std::string> seen_ids;
  for (auto iter = results.begin(); iter != results.end();) {
    bool inserted = seen_ids.insert((*iter)->id()).second;
    if (!inserted) {
      // C++11:: The return value of erase(iter) is an iterator pointing to the
      // next element in the container.
      iter = results.erase(iter);
    } else {
      ++iter;
    }
  }
}

}  //  namespace

OmniboxProvider::OmniboxProvider(Profile* profile,
                                 AppListControllerDelegate* list_controller)
    : profile_(profile),
      list_controller_(list_controller),
      controller_(std::make_unique<AutocompleteController>(
          std::make_unique<ChromeAutocompleteProviderClient>(profile),
          ProviderTypes())),
      favicon_cache_(FaviconServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS),
                     HistoryServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS)) {
  controller_->AddObserver(this);
}

OmniboxProvider::~OmniboxProvider() {}

void OmniboxProvider::Start(const std::u16string& query) {
  ClearResultsSilently();
  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kCamelCase);

  controller_->Stop(false);
  query_finished_ = false;
  // The new page classification value(CHROMEOS_APP_LIST) is introduced
  // to differentiate the suggest requests initiated by ChromeOS app_list from
  // the ones by Chrome omnibox.
  input_ =
      AutocompleteInput(query, metrics::OmniboxEventProto::CHROMEOS_APP_LIST,
                        ChromeAutocompleteSchemeClassifier(profile_));

  // Sets the |from_omnibox_focus| flag to enable ZeroSuggestProvider to process
  // the requests from app_list.
  if (input_.text().empty()) {
    input_.set_focus_type(OmniboxFocusType::ON_FOCUS);
    is_zero_state_input_ = true;
  } else {
    is_zero_state_input_ = false;
  }

  query_start_time_ = base::TimeTicks::Now();
  controller_->Start(input_);
}

void OmniboxProvider::StartZeroState() {
  // Do not perform zero-state queries in the productivity launcher, because
  // Omnibox is not shown in zero-state.
  if (!app_list_features::IsCategoricalSearchEnabled()) {
    Start(std::u16string());
  }
}

ash::AppListSearchResultType OmniboxProvider::ResultType() const {
  return ash::AppListSearchResultType::kOmnibox;
}

void OmniboxProvider::PopulateFromACResult(const AutocompleteResult& result) {
  SearchProvider::Results new_results;
  new_results.reserve(result.size());

  std::vector<std::unique_ptr<OmniboxResult>> list_results;
  list_results.reserve(result.size());

  for (const AutocompleteMatch& match : result) {
    // Do not return a match in any of these cases:
    // - The URL is invalid.
    // - The URL points to Drive Web. The Drive search and zero-state
    //   providers surface Drive results.
    // - The URL points to a local file. The Local file search and zero-state
    //   providers handle local file results, even if they've been opened in
    //   the browser.
    if (!match.destination_url.is_valid() ||
        IsDriveUrl(match.destination_url) ||
        match.destination_url.SchemeIsFile()) {
      continue;
    }

    if (!is_zero_state_input_ && IsAnswer(match) &&
        !ShouldFilterAnswer(match, last_query_)) {
      new_results.emplace_back(std::make_unique<OmniboxAnswerResult>(
          profile_, list_controller_, controller_.get(), match, last_query_));
    } else if (match.type == AutocompleteMatchType::OPEN_TAB) {
      DCHECK(last_tokenized_query_.has_value());
      new_results.emplace_back(std::make_unique<OpenTabResult>(
          profile_, list_controller_, &favicon_cache_,
          last_tokenized_query_.value(), match));
    } else {
      list_results.emplace_back(std::make_unique<OmniboxResult>(
          profile_, list_controller_, controller_.get(), &favicon_cache_,
          input_, match, is_zero_state_input_));
    }
  }

  // Deduplicate the list results and then move-concatenate it into new_results.
  RemoveDuplicates(list_results);
  std::move(list_results.begin(), list_results.end(),
            std::back_inserter(new_results));

  if (controller_->done() && !query_finished_) {
    query_finished_ = true;
    SwapResults(&new_results);
  }
}

void OmniboxProvider::OnResultChanged(AutocompleteController* controller,
                                      bool default_match_changed) {
  DCHECK(controller == controller_.get());

  // Record the query latency.
  RecordQueryLatencyHistogram();

  PopulateFromACResult(controller_->result());
}

void OmniboxProvider::RecordQueryLatencyHistogram() {
  base::TimeDelta query_latency = base::TimeTicks::Now() - query_start_time_;
  if (is_zero_state_input_) {
    UMA_HISTOGRAM_TIMES("Apps.AppList.OmniboxProvider.ZeroStateLatency",
                        query_latency);
  } else {
    UMA_HISTOGRAM_TIMES("Apps.AppList.OmniboxProvider.QueryTime",
                        query_latency);
  }
}

}  // namespace app_list
