// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/reading_list/reading_list_suggestions_provider.h"

#include <memory>

#include "base/test/simple_test_clock.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace {

const char kTitleUnread1[] = "title1";
const char kTitleUnread2[] = "title2";
const char kTitleUnread3[] = "title3";
const char kTitleUnread4[] = "title4";
const char kTitleRead1[] = "title_read1";

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Property;

class ReadingListSuggestionsProviderTest : public ::testing::Test {
 public:
  ReadingListSuggestionsProviderTest() {
    model_ = std::make_unique<ReadingListModelImpl>(
        /*storage_layer=*/nullptr, /*pref_service=*/nullptr, &clock_);
  }

  void CreateProvider() {
    EXPECT_CALL(observer_,
                OnCategoryStatusChanged(_, ReadingListCategory(),
                                        CategoryStatus::AVAILABLE_LOADING))
        .RetiresOnSaturation();
    EXPECT_CALL(observer_, OnCategoryStatusChanged(_, ReadingListCategory(),
                                                   CategoryStatus::AVAILABLE))
        .RetiresOnSaturation();
    provider_ = std::make_unique<ReadingListSuggestionsProvider>(&observer_,
                                                                 model_.get());
  }

  Category ReadingListCategory() {
    return Category::FromKnownCategory(KnownCategories::READING_LIST);
  }

  void AddEntries() {
    model_->AddEntry(url_unread1_, kTitleUnread1,
                     reading_list::ADDED_VIA_CURRENT_APP);
    clock_.Advance(base::Milliseconds(10));
    model_->AddEntry(url_unread2_, kTitleUnread2,
                     reading_list::ADDED_VIA_CURRENT_APP);
    clock_.Advance(base::Milliseconds(10));
    model_->AddEntry(url_read1_, kTitleRead1,
                     reading_list::ADDED_VIA_CURRENT_APP);
    model_->SetReadStatus(url_read1_, true);
    clock_.Advance(base::Milliseconds(10));
    model_->AddEntry(url_unread3_, kTitleUnread3,
                     reading_list::ADDED_VIA_CURRENT_APP);
    clock_.Advance(base::Milliseconds(10));
    model_->AddEntry(url_unread4_, kTitleUnread4,
                     reading_list::ADDED_VIA_CURRENT_APP);
  }

 protected:
  base::SimpleTestClock clock_;
  std::unique_ptr<ReadingListModelImpl> model_;
  testing::StrictMock<MockContentSuggestionsProviderObserver> observer_;
  std::unique_ptr<ReadingListSuggestionsProvider> provider_;

  const GURL url_unread1_{"http://www.foo1.bar"};
  const GURL url_unread2_{"http://www.foo2.bar"};
  const GURL url_unread3_{"http://www.foo3.bar"};
  const GURL url_unread4_{"http://www.foo4.bar"};
  const GURL url_read1_{"http://www.bar.foor"};
};

TEST_F(ReadingListSuggestionsProviderTest, CategoryInfo) {
  EXPECT_CALL(observer_, OnNewSuggestions(_, ReadingListCategory(), IsEmpty()))
      .RetiresOnSaturation();
  CreateProvider();

  CategoryInfo categoryInfo = provider_->GetCategoryInfo(ReadingListCategory());
  EXPECT_EQ(ContentSuggestionsAdditionalAction::VIEW_ALL,
            categoryInfo.additional_action());
}

TEST_F(ReadingListSuggestionsProviderTest, ReturnsThreeLatestUnreadSuggestion) {
  AddEntries();

  EXPECT_CALL(
      observer_,
      OnNewSuggestions(
          _, ReadingListCategory(),
          ElementsAre(Property(&ContentSuggestion::url, url_unread4_),
                      Property(&ContentSuggestion::url, url_unread3_),
                      Property(&ContentSuggestion::url, url_unread2_))));

  CreateProvider();
}

// Tests that the provider returns only unread suggestions even if there is less
// unread suggestions than the maximum number of suggestions.
TEST_F(ReadingListSuggestionsProviderTest, ReturnsOnlyUnreadSuggestion) {
  GURL url_unread1 = GURL("http://www.foo1.bar");
  GURL url_read1 = GURL("http://www.bar.foor");
  std::string title_unread1 = "title1";
  std::string title_read1 = "title_read1";
  model_->AddEntry(url_unread1, title_unread1,
                   reading_list::ADDED_VIA_CURRENT_APP);
  clock_.Advance(base::Milliseconds(10));
  model_->AddEntry(url_read1, title_read1, reading_list::ADDED_VIA_CURRENT_APP);
  model_->SetReadStatus(url_read1, true);

  EXPECT_CALL(observer_,
              OnNewSuggestions(
                  _, ReadingListCategory(),
                  ElementsAre(Property(&ContentSuggestion::url, url_unread1))));

  CreateProvider();
}

TEST_F(ReadingListSuggestionsProviderTest, DismissesEntry) {
  AddEntries();

  EXPECT_CALL(
      observer_,
      OnNewSuggestions(
          _, ReadingListCategory(),
          ElementsAre(Property(&ContentSuggestion::url, url_unread4_),
                      Property(&ContentSuggestion::url, url_unread3_),
                      Property(&ContentSuggestion::url, url_unread2_))));

  CreateProvider();

  EXPECT_CALL(
      observer_,
      OnNewSuggestions(
          _, ReadingListCategory(),
          ElementsAre(Property(&ContentSuggestion::url, url_unread4_),
                      Property(&ContentSuggestion::url, url_unread2_),
                      Property(&ContentSuggestion::url, url_unread1_))));

  provider_->DismissSuggestion(
      ContentSuggestion::ID(ReadingListCategory(), url_unread3_.spec()));
}

}  // namespace

}  // namespace ntp_snippets
