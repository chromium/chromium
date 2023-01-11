// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ntp_snippets {

// TODO(treib): This is a weird combination of a mock and a fake. Fix this.
class MockContentSuggestionsProvider : public ContentSuggestionsProvider {
 public:
  using DestructorCallback = base::OnceCallback<void()>;

  MockContentSuggestionsProvider(
      Observer* observer,
      const std::vector<Category>& provided_categories);
  MockContentSuggestionsProvider(const MockContentSuggestionsProvider&) =
      delete;
  MockContentSuggestionsProvider& operator=(
      const MockContentSuggestionsProvider&) = delete;
  ~MockContentSuggestionsProvider() override;

  void SetProvidedCategories(const std::vector<Category>& provided_categories);

  // Returns the status for |category|. The initial status in
  // CatgoryStatus::AVAILABLE. Will be updated on FireCategoryStatusChanged
  // events.
  CategoryStatus GetCategoryStatus(Category category) override;

  // Returns a hard-coded category info object.
  CategoryInfo GetCategoryInfo(Category category) override;

  // Forwards events to the underlying oberservers.
  // TODO(tschumann): This functionality does not belong here. Whoever injected
  // the observer into the constructor can as well notify the observer itself.
  void FireSuggestionsChanged(Category category,
                              std::vector<ContentSuggestion> suggestions);
  void FireCategoryStatusChanged(Category category, CategoryStatus new_status);
  void FireCategoryStatusChangedWithCurrentStatus(Category category);
  void FireSuggestionInvalidated(const ContentSuggestion::ID& suggestion_id);

  // Set a callback to be called in the destructor. Used to "mock" destruction.
  void SetDestructorCallback(DestructorCallback callback);

  MOCK_METHOD3(
      ClearHistory,
      void(base::Time begin,
           base::Time end,
           const base::RepeatingCallback<bool(const GURL& url)>& filter));
  // Gmock cannot mock methods that have movable-only type callbacks as
  // parameters such as FetchDoneCallback, DismissedSuggestionsCallback,
  // ImageFetchedCallback. As a work-around, Fetch calls the mock method
  // FetchMock, which may then be checked with EXPECT_CALL.
  void Fetch(const Category&,
             const std::set<std::string>&,
             FetchDoneCallback) override;
  MOCK_METHOD3(FetchMock,
               void(const Category&,
                    const std::set<std::string>&,
                    FetchDoneCallback*));
  MOCK_METHOD0(ClearCachedSuggestions, void());
  void GetDismissedSuggestionsForDebugging(
      Category category,
      DismissedSuggestionsCallback callback) override;
  MOCK_METHOD2(GetDismissedSuggestionsForDebuggingMock,
               void(Category category,
                    const DismissedSuggestionsCallback& callback));
  MOCK_METHOD1(ClearDismissedSuggestionsForDebugging, void(Category category));
  MOCK_METHOD1(DismissSuggestion,
               void(const ContentSuggestion::ID& suggestion_id));
  void FetchSuggestionImage(const ContentSuggestion::ID& id,
                            ImageFetchedCallback callback) override;
  void FetchSuggestionImageData(const ContentSuggestion::ID& id,
                                ImageDataFetchedCallback callback) override;
  MOCK_METHOD2(FetchSuggestionImageMock,
               void(const ContentSuggestion::ID&, const ImageFetchedCallback&));
  MOCK_METHOD2(FetchSuggestionImageDataMock,
               void(const ContentSuggestion::ID&, ImageDataFetchedCallback*));

 private:
  std::vector<Category> provided_categories_;
  std::map<int, CategoryStatus> statuses_;

  DestructorCallback destructor_callback_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_MOCK_CONTENT_SUGGESTIONS_PROVIDER_H_
