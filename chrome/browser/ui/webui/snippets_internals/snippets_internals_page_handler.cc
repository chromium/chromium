// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/snippets_internals/snippets_internals_page_handler.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals.mojom.h"
#include "chrome/common/pref_names.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/variations/variations_associated_data.h"

using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::RemoteSuggestionsProvider;
using ntp_snippets::RemoteSuggestionsFetcher;
using ntp_snippets::UserClassifier;

namespace {
/*
  Non-instance helper functions.
*/

std::set<variations::VariationID> GetSnippetsExperiments() {
  std::set<variations::VariationID> result;
  for (const base::Feature* feature : ntp_snippets::GetAllFeatures()) {
    base::FieldTrial* trial = base::FeatureList::GetFieldTrial(*feature);
    if (!trial || trial->GetGroupNameWithoutActivation().empty())
      continue;

    for (variations::IDCollectionKey key :
         {variations::GOOGLE_WEB_PROPERTIES,
          variations::GOOGLE_WEB_PROPERTIES_SIGNED_IN,
          variations::GOOGLE_WEB_PROPERTIES_TRIGGER}) {
      const variations::VariationID id = variations::GetGoogleVariationID(
          key, trial->trial_name(), trial->group_name());
      if (id != variations::EMPTY_ID) {
        result.insert(id);
      }
    }
  }
  return result;
}

std::string BooleanToString(bool value) {
  return value ? "True" : "False";
}

std::string GetCategoryStatusName(CategoryStatus status) {
  switch (status) {
    case CategoryStatus::INITIALIZING:
      return "INITIALIZING";
    case CategoryStatus::AVAILABLE:
      return "AVAILABLE";
    case CategoryStatus::AVAILABLE_LOADING:
      return "AVAILABLE_LOADING";
    case CategoryStatus::NOT_PROVIDED:
      return "NOT_PROVIDED";
    case CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED:
      return "ALL_SUGGESTIONS_EXPLICITLY_DISABLED";
    case CategoryStatus::CATEGORY_EXPLICITLY_DISABLED:
      return "CATEGORY_EXPLICITLY_DISABLED";
    case CategoryStatus::LOADING_ERROR:
      return "LOADING_ERROR";
  }
  return std::string();
}

snippets_internals::mojom::SuggestionItemPtr PrepareContentSuggestionItem(
    const ContentSuggestion& suggestion,
    int index) {
  auto item = snippets_internals::mojom::SuggestionItem::New();
  item->suggestionTitle = base::UTF16ToUTF8(suggestion.title());
  item->suggestionIdWithinCategory = suggestion.id().id_within_category();
  item->suggestionId = "content-suggestion-" + base::NumberToString(index);
  item->url = suggestion.url().spec();
  item->faviconUrl = suggestion.url_with_favicon().spec();
  item->snippet = base::UTF16ToUTF8(suggestion.snippet_text());
  item->publishDate =
      base::UTF16ToUTF8(TimeFormatShortDateAndTime(suggestion.publish_date()));
  item->publisherName = base::UTF16ToUTF8(suggestion.publisher_name());
  item->score = suggestion.score();

  return item;
}

}  // namespace

// TODO: Add browser tests.
SnippetsInternalsPageHandler::SnippetsInternalsPageHandler(
    mojo::PendingReceiver<snippets_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<snippets_internals::mojom::Page> page,
    ntp_snippets::ContentSuggestionsService* content_suggestions_service,
    PrefService* pref_service)
    : receiver_(this, std::move(receiver)),
      content_suggestions_service_observer_(this),
      content_suggestions_service_(content_suggestions_service),
      remote_suggestions_provider_(
          content_suggestions_service_
              ->remote_suggestions_provider_for_debugging()),
      pref_service_(pref_service),
      page_(std::move(page)) {}

SnippetsInternalsPageHandler::~SnippetsInternalsPageHandler() {}

/*
  Observer methods.
*/

void SnippetsInternalsPageHandler::OnNewSuggestions(Category category) {
  page_->OnSuggestionsChanged();
}

void SnippetsInternalsPageHandler::OnCategoryStatusChanged(
    Category category,
    CategoryStatus new_status) {
  page_->OnSuggestionsChanged();
}

void SnippetsInternalsPageHandler::OnSuggestionInvalidated(
    const ntp_snippets::ContentSuggestion::ID& suggestion_id) {
  page_->OnSuggestionsChanged();
}

void SnippetsInternalsPageHandler::OnFullRefreshRequired() {
  page_->OnSuggestionsChanged();
}

void SnippetsInternalsPageHandler::ContentSuggestionsServiceShutdown() {}

/*
  Instance methods.
*/

void SnippetsInternalsPageHandler::GetGeneralProperties(
    GetGeneralPropertiesCallback callback) {
  auto properties = base::flat_map<std::string, std::string>();
  properties["flag-article-suggestions"] = BooleanToString(
      base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature));
  properties["flag-offlining-recent-pages-feature"] =
      BooleanToString(base::FeatureList::IsEnabled(
          offline_pages::kOffliningRecentPagesFeature));

  if (remote_suggestions_provider_) {
    const ntp_snippets::RemoteSuggestionsFetcher* fetcher =
        remote_suggestions_provider_->suggestions_fetcher_for_debugging();
    properties["switch-fetch-url"] = fetcher->GetFetchUrlForDebugging().spec();
  }

  std::set<variations::VariationID> ids = GetSnippetsExperiments();
  std::vector<std::string> string_ids;
  std::transform(
      ids.begin(), ids.end(), std::back_inserter(string_ids),
      [](variations::VariationID id) { return base::NumberToString(id); });

  properties["experiment-ids"] = base::JoinString(string_ids, ", ");
  std::move(callback).Run(properties);
}

void SnippetsInternalsPageHandler::GetUserClassifierProperties(
    GetUserClassifierPropertiesCallback callback) {
  auto properties = base::flat_map<std::string, std::string>();
  properties["user-class"] = content_suggestions_service_->user_classifier()
                                 ->GetUserClassDescriptionForDebugging();
  properties["avg-time-to-open-ntp"] = base::NumberToString(
      content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
          UserClassifier::Metric::NTP_OPENED));
  properties["avg-time-to-show"] = base::NumberToString(
      content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
          UserClassifier::Metric::SUGGESTIONS_SHOWN));
  properties["avg-time-to-use"] = base::NumberToString(
      content_suggestions_service_->user_classifier()->GetEstimatedAvgTime(
          UserClassifier::Metric::SUGGESTIONS_USED));
  std::move(callback).Run(properties);
}

void SnippetsInternalsPageHandler::ClearUserClassifierProperties() {
  content_suggestions_service_->user_classifier()
      ->ClearClassificationForDebugging();
}

void SnippetsInternalsPageHandler::GetCategoryRankerProperties(
    GetCategoryRankerPropertiesCallback callback) {
  auto properties = base::flat_map<std::string, std::string>();
  std::vector<ntp_snippets::CategoryRanker::DebugDataItem> data =
      content_suggestions_service_->category_ranker()->GetDebugData();

  for (const auto& item : data) {
    properties[item.label] = item.content;
  }

  std::move(callback).Run(properties);
}

void SnippetsInternalsPageHandler::ReloadSuggestions() {
  if (remote_suggestions_provider_) {
    remote_suggestions_provider_->ReloadSuggestions();
  }
}

void SnippetsInternalsPageHandler::ClearCachedSuggestions() {
  content_suggestions_service_->ClearAllCachedSuggestions();
  page_->OnSuggestionsChanged();
}

void SnippetsInternalsPageHandler::GetRemoteContentSuggestionsProperties(
    GetRemoteContentSuggestionsPropertiesCallback callback) {
  auto properties = base::flat_map<std::string, std::string>();
  if (remote_suggestions_provider_) {
    const std::string& status =
        remote_suggestions_provider_->suggestions_fetcher_for_debugging()
            ->GetLastStatusForDebugging();
    if (!status.empty()) {
      properties["remote-status"] = "Finished: " + status;
      properties["remote-authenticated"] =
          remote_suggestions_provider_->suggestions_fetcher_for_debugging()
                  ->WasLastFetchAuthenticatedForDebugging()
              ? "Authenticated"
              : "Non-authenticated";
    }
  }

  base::Time time = base::Time::FromInternalValue(pref_service_->GetInt64(
      ntp_snippets::prefs::kLastSuccessfulBackgroundFetchTime));
  properties["last-background-fetch-time"] =
      base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));

  std::move(callback).Run(properties);
}

void SnippetsInternalsPageHandler::FetchSuggestionsInBackground(
    int64_t delaySeconds,
    FetchSuggestionsInBackgroundCallback callback) {
  DCHECK(delaySeconds >= 0);
  suggestion_fetch_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(delaySeconds),
      base::BindRepeating(
          &SnippetsInternalsPageHandler::FetchSuggestionsInBackgroundImpl,
          weak_ptr_factory_.GetWeakPtr(), base::Passed(std::move(callback))));
}

void SnippetsInternalsPageHandler::FetchSuggestionsInBackgroundImpl(
    FetchSuggestionsInBackgroundCallback callback) {
  remote_suggestions_provider_->RefetchInTheBackground(
      RemoteSuggestionsProvider::FetchStatusCallback());

  std::move(callback).Run();
}

void SnippetsInternalsPageHandler::GetLastJson(GetLastJsonCallback callback) {
  std::string json = "";
  if (remote_suggestions_provider_) {
    const ntp_snippets::RemoteSuggestionsFetcher* fetcher =
        remote_suggestions_provider_->suggestions_fetcher_for_debugging();
    json = fetcher->GetLastJsonForDebugging();
  }

  std::move(callback).Run(json);
}

void SnippetsInternalsPageHandler::GetSuggestionsByCategory(
    GetSuggestionsByCategoryCallback callback) {
  CollectDismissedSuggestions(-1, std::move(callback),
                              std::vector<ContentSuggestion>());
}

void SnippetsInternalsPageHandler::GetSuggestionsByCategoryImpl(
    GetSuggestionsByCategoryCallback callback) {
  std::vector<snippets_internals::mojom::SuggestionCategoryPtr> categories;

  int index = 0;
  for (Category category : content_suggestions_service_->GetCategories()) {
    CategoryStatus status =
        content_suggestions_service_->GetCategoryStatus(category);
    base::Optional<CategoryInfo> info =
        content_suggestions_service_->GetCategoryInfo(category);
    DCHECK(info);
    const std::vector<ContentSuggestion>& suggestions =
        content_suggestions_service_->GetSuggestionsForCategory(category);

    std::vector<snippets_internals::mojom::SuggestionItemPtr> items;
    for (const ContentSuggestion& suggestion : suggestions) {
      snippets_internals::mojom::SuggestionItemPtr item =
          PrepareContentSuggestionItem(suggestion, index++);
      items.push_back(std::move(item));
    }

    std::vector<snippets_internals::mojom::SuggestionItemPtr> dismissed_items;
    for (const ContentSuggestion& suggestion :
         dismissed_suggestions_[category]) {
      snippets_internals::mojom::SuggestionItemPtr item =
          PrepareContentSuggestionItem(suggestion, index++);
      dismissed_items.push_back(std::move(item));
    }

    auto suggestion_category =
        snippets_internals::mojom::SuggestionCategory::New();
    suggestion_category->categoryTitle = base::UTF16ToUTF8(info->title());
    suggestion_category->status = GetCategoryStatusName(status);
    suggestion_category->categoryId = category.id();
    suggestion_category->suggestions = std::move(items);
    suggestion_category->dismissedSuggestions = std::move(dismissed_items);
    categories.push_back(std::move(suggestion_category));
  }

  std::move(callback).Run(std::move(categories));
}

void SnippetsInternalsPageHandler::ClearDismissedSuggestions(
    int64_t category_id) {
  Category category = Category::FromIDValue(category_id);
  content_suggestions_service_->ClearDismissedSuggestionsForDebugging(category);
  page_->OnSuggestionsChanged();
}

void SnippetsInternalsPageHandler::CollectDismissedSuggestions(
    int last_index,
    GetSuggestionsByCategoryCallback callback,
    std::vector<ContentSuggestion> suggestions) {
  std::vector<Category> categories =
      content_suggestions_service_->GetCategories();

  // Populate our last category results.
  if (last_index > -1)
    dismissed_suggestions_[categories[last_index]] = std::move(suggestions);

  // Find the next category for this.
  for (size_t i = 0; i < categories.size(); i++) {
    // Continue the process in the next method call.
    if (last_index + 1 >= 0 && (size_t)last_index + 1 == i) {
      content_suggestions_service_->GetDismissedSuggestionsForDebugging(
          categories[i],
          base::BindRepeating(
              &SnippetsInternalsPageHandler::CollectDismissedSuggestions,
              weak_ptr_factory_.GetWeakPtr(), i,
              base::Passed(std::move(callback))));
      return;
    }
  }

  // Call into impl once the dismissed categories have been collected.
  GetSuggestionsByCategoryImpl(std::move(callback));
}
