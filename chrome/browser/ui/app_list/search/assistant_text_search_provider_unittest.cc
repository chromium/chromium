// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/assistant_text_search_provider.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/assistant/test_support/mock_assistant_controller.h"
#include "ash/public/cpp/assistant/test_support/mock_assistant_state.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using ::ash::assistant::AssistantAllowedState;

// Parameterized by feature ProductivityLauncher.
class AssistantTextSearchProviderTest
    : public AppListTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  AssistantTextSearchProviderTest() {
    feature_list_.InitWithFeatureState(ash::features::kProductivityLauncher,
                                       GetParam());
    auto search_provider = std::make_unique<AssistantTextSearchProvider>();
    search_provider_ = search_provider.get();
    search_controller_.AddProvider(0, std::move(search_provider));
  }
  AssistantTextSearchProviderTest(const AssistantTextSearchProviderTest&) =
      delete;
  AssistantTextSearchProviderTest& operator=(
      const AssistantTextSearchProviderTest&) = delete;
  ~AssistantTextSearchProviderTest() override = default;

  void SendText(const std::string& text) {
    search_controller_.StartSearch(base::UTF8ToUTF16(text));
  }

  ash::MockAssistantState& assistant_state() { return assistant_state_; }

  testing::NiceMock<ash::MockAssistantController>& assistant_controller() {
    return assistant_controller_;
  }

  const SearchProvider::Results& LastResults() {
    if (app_list_features::IsCategoricalSearchEnabled()) {
      return search_controller_.last_results();
    } else {
      return search_provider_->results();
    }
  }

  void VerifyResultAt(size_t index, const std::string& text) {
    EXPECT_LT(index, LastResults().size());
    auto* result = LastResults().at(0).get();
    EXPECT_EQ(result->title(), base::UTF8ToUTF16(text));
    EXPECT_EQ(result->details(), u"Google Assistant");
    EXPECT_EQ(result->id(), "googleassistant_text://" + text);
    EXPECT_EQ(result->accessible_name(),
              base::UTF8ToUTF16(text + ", Google Assistant"));
    EXPECT_EQ(result->result_type(),
              ash::AppListSearchResultType::kAssistantText);
    EXPECT_EQ(result->display_type(), ash::SearchResultDisplayType::kList);
    EXPECT_EQ(result->skip_update_animation(), true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ash::MockAssistantState assistant_state_;
  testing::NiceMock<ash::MockAssistantController> assistant_controller_;
  TestSearchController search_controller_;
  AssistantTextSearchProvider* search_provider_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(ProductivityLauncher,
                         AssistantTextSearchProviderTest,
                         testing::Bool());

// Tests -----------------------------------------------------------------------

// TODO(crbug.com/1258415): Remove this test when the productivity launcher is
// enabled.
TEST_P(AssistantTextSearchProviderTest, ShouldNotProvideResultForEmptyQuery) {
  EXPECT_TRUE(LastResults().empty());

  SendText("testing");
  // Should now have a search result with title "testing".
  EXPECT_EQ(LastResults().size(), 1u);
  VerifyResultAt(0, "testing");

  // If the productivity launcher is enabled, search_provider_.Start() is
  // guaranteed to be called with a non-empty query. So this test only applies
  // to the classic launcher.
  bool productivity_launcher_enabled = GetParam();
  if (!productivity_launcher_enabled) {
    SendText("");
    // Should have no search results.
    EXPECT_TRUE(LastResults().empty());
  }
}

TEST_P(AssistantTextSearchProviderTest,
       ShouldUpdateResultsWhenAssistantSettingsChange) {
  SendText("testing");
  EXPECT_EQ(LastResults().size(), 1u);

  assistant_state().SetSettingsEnabled(false);
  EXPECT_TRUE(LastResults().empty());

  assistant_state().SetSettingsEnabled(true);
  EXPECT_EQ(LastResults().size(), 1u);
}

TEST_P(AssistantTextSearchProviderTest,
       ShouldUpdateResultsWhenAssistantAllowedStateChanges) {
  SendText("testing");

  // Test all possible Assistant allowed states.
  for (int i = 0; i < static_cast<int>(AssistantAllowedState::MAX_VALUE); ++i) {
    if (i == static_cast<int>(AssistantAllowedState::ALLOWED))
      continue;

    // When Assistant becomes not-allowed, results should be cleared.
    assistant_state().SetAllowedState(static_cast<AssistantAllowedState>(i));
    EXPECT_TRUE(LastResults().empty());

    // When Assistant becomes allowed, we should again have a single result.
    assistant_state().SetAllowedState(AssistantAllowedState::ALLOWED);
    EXPECT_EQ(1u, LastResults().size());
  }
}
TEST_P(AssistantTextSearchProviderTest, ShouldDeepLinkAssistantQuery) {
  SendText("testing query");

  GURL url;
  bool in_background = true;
  bool from_user = true;
  EXPECT_CALL(assistant_controller(), OpenUrl)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&url),
                               testing::SaveArg<1>(&in_background),
                               testing::SaveArg<2>(&from_user)));

  LastResults().at(0)->Open(/*event_flags=*/0);
  EXPECT_EQ(url, GURL("googleassistant://send-query?q=testing+query"));
  EXPECT_FALSE(in_background);
  EXPECT_FALSE(from_user);
}

}  // namespace
}  // namespace app_list
