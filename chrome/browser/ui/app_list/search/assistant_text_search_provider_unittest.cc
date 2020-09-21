// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/assistant_text_search_provider.h"

#include "ash/public/cpp/assistant/test_support/mock_assistant_controller.h"
#include "ash/public/cpp/assistant/test_support/mock_assistant_state.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace app_list {
namespace test {

using chromeos::assistant::AssistantAllowedState;

class AssistantTextSearchProviderTest : public AppListTestBase {
 public:
  AssistantTextSearchProviderTest() = default;
  AssistantTextSearchProviderTest(const AssistantTextSearchProviderTest&) =
      delete;
  AssistantTextSearchProviderTest& operator=(
      const AssistantTextSearchProviderTest&) = delete;
  ~AssistantTextSearchProviderTest() override = default;

  void SendText(const std::string& text) {
    search_provider_.Start(base::UTF8ToUTF16(text));
  }

  AssistantTextSearchProvider& search_provider() { return search_provider_; }

  ash::MockAssistantState& assistant_state() { return assistant_state_; }

  testing::NiceMock<ash::MockAssistantController>& assistant_controller() {
    return assistant_controller_;
  }

  void VerifyResultAt(size_t index, const std::string& text) {
    EXPECT_LT(index, search_provider().results().size());
    auto* result = search_provider().results().at(0).get();
    EXPECT_EQ(result->title(), base::UTF8ToUTF16(text));
    EXPECT_EQ(result->id(), "googleassistant_text://" + text);
    EXPECT_EQ(result->accessible_name(),
              base::UTF8ToUTF16(text + ", Google Assistant"));
    EXPECT_EQ(result->result_type(),
              ash::AppListSearchResultType::kAssistantText);
    EXPECT_EQ(result->display_type(), ash::SearchResultDisplayType::kList);
  }

 private:
  ash::MockAssistantState assistant_state_;
  testing::NiceMock<ash::MockAssistantController> assistant_controller_;
  AssistantTextSearchProvider search_provider_;
};

// Tests -----------------------------------------------------------------------

TEST_F(AssistantTextSearchProviderTest, ShouldNotProvideResultForEmptyQuery) {
  EXPECT_TRUE(search_provider().results().empty());

  SendText("testing");
  // Should now have a search result with title "testing".
  EXPECT_EQ(search_provider().results().size(), 1u);
  VerifyResultAt(0, "testing");

  SendText("");
  // Should have no search results.
  EXPECT_TRUE(search_provider().results().empty());
}

TEST_F(AssistantTextSearchProviderTest,
       ShouldUpdateResultsWhenAssistantSettingsChange) {
  SendText("testing");
  EXPECT_EQ(search_provider().results().size(), 1u);

  assistant_state().SetSettingsEnabled(false);
  EXPECT_TRUE(search_provider().results().empty());

  assistant_state().SetSettingsEnabled(true);
  EXPECT_EQ(search_provider().results().size(), 1u);
}

TEST_F(AssistantTextSearchProviderTest,
       ShouldUpdateResultsWhenAssistantAllowedStateChanges) {
  SendText("testing");

  // Test all possible Assistant allowed states.
  for (int i = 0; i < static_cast<int>(AssistantAllowedState::MAX_VALUE); ++i) {
    if (i == static_cast<int>(AssistantAllowedState::ALLOWED))
      continue;

    // When Assistant becomes not-allowed, results should be cleared.
    assistant_state().SetAllowedState(static_cast<AssistantAllowedState>(i));
    EXPECT_TRUE(search_provider().results().empty());

    // When Assistant becomes allowed, we should again have a single result.
    assistant_state().SetAllowedState(AssistantAllowedState::ALLOWED);
    EXPECT_EQ(1u, search_provider().results().size());
  }
}
TEST_F(AssistantTextSearchProviderTest, ShouldDeepLinkAssistantQuery) {
  SendText("testing query");

  GURL url;
  bool in_background = true;
  bool from_user = true;
  EXPECT_CALL(assistant_controller(), OpenUrl)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&url),
                               testing::SaveArg<1>(&in_background),
                               testing::SaveArg<2>(&from_user)));

  search_provider().results().at(0)->Open(/*event_flags=*/0);
  EXPECT_EQ(url, GURL("googleassistant://send-query?q=testing+query"));
  EXPECT_FALSE(in_background);
  EXPECT_FALSE(from_user);
}

}  // namespace test
}  // namespace app_list
