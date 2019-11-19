// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_READING_LIST_READING_LIST_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_READING_LIST_READING_LIST_SUGGESTIONS_PROVIDER_H_

#include <set>
#include <string>

#include "base/scoped_observer.h"
#include "components/ntp_snippets/callbacks.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"

namespace ntp_snippets {

// Provides content suggestions from the Reading List.
class ReadingListSuggestionsProvider : public ContentSuggestionsProvider,
                                       public ReadingListModelObserver {
 public:
  ReadingListSuggestionsProvider(ContentSuggestionsProvider::Observer* observer,
                                 ReadingListModel* reading_list_model);
  ~ReadingListSuggestionsProvider() override;

  // ContentSuggestionsProvider implementation.
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            ImageFetchedCallback callback) override;
  void FetchSuggestionImageData(const ContentSuggestion::ID& suggestion_id,
                                ImageDataFetchedCallback callback) override;
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             FetchDoneCallback callback) override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions() override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      DismissedSuggestionsCallback callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

  // ReadingListModelObserver implementation.
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override;

 private:
  // The actual method to fetch Reading List entries. Must be called after the
  // model is loaded.
  void FetchReadingListInternal();

  // Converts |entry| to ContentSuggestion.
  ContentSuggestion ConvertEntry(const ReadingListEntry* entry);

  // Updates the |category_status_| and notifies the |observer_|, if necessary.
  void NotifyStatusChanged(CategoryStatus new_status);

  // Sets the dismissed status of the entry to |dismissed|.
  void SetDismissedState(const GURL& url, bool dismissed);

  CategoryStatus category_status_;
  const Category provided_category_;

  ReadingListModel* reading_list_model_;
  ScopedObserver<ReadingListModel, ReadingListModelObserver> scoped_observer_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ReadingListSuggestionsProvider);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_READING_LIST_READING_LIST_SUGGESTIONS_PROVIDER_H_
