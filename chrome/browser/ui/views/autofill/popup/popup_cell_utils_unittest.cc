// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"

namespace autofill {
namespace {

const char* GetExpandableMenuIconNameFromSuggestionType(SuggestionType type) {
  return popup_cell_utils::GetExpandableMenuIcon(type).name;
}

TEST(PopupCellUtilsTest,
     GetExpandableMenuIcon_ComposeSuggestions_ReturnThreeDotsMenuIcon) {
  EXPECT_EQ(GetExpandableMenuIconNameFromSuggestionType(
                SuggestionType::kComposeProactiveNudge),
            kBrowserToolsChromeRefreshIcon.name);
  // No other Compose type should allow an expandable menu.
  EXPECT_FALSE(IsExpandableSuggestionType(SuggestionType::kComposeResumeNudge));
  EXPECT_FALSE(IsExpandableSuggestionType(
      SuggestionType::kComposeSavedStateNotification));
}

TEST(PopupCellUtilsTest,
     GetExpandableMenuIcon_NonComposeSuggestions_ReturnSubMenuArrowIcon) {
  EXPECT_EQ(GetExpandableMenuIconNameFromSuggestionType(
                SuggestionType::kAddressEntry),
            vector_icons::kSubmenuArrowChromeRefreshIcon.name);
}

// Tests that if a throbber is used instead of an icon the preferred size of the
// `PopupRowContentView` does not change.
TEST(PopupCellUtilsTest, SettingIsLoadingMaintainsPreferredSize) {
  // Needed for the throbber.
  base::test::TaskEnvironment task_environment;
  // Needed to construct a `PopupRowContentView`.
  ChromeLayoutProvider layout_provider;
  Suggestion suggestion(SuggestionType::kCreateNewPlusAddressInline);
  suggestion.icon = Suggestion::Icon::kPlusAddress;

  auto make_main_label = []() {
    return std::make_unique<views::Label>(u"Create new plus address");
  };
  auto make_icon = [&]() {
    return popup_cell_utils::GetIconImageView(suggestion);
  };

  // Ensure that the test is meaningful: The throbber and the icon should have
  // different minimum sizes.
  ASSERT_NE(std::make_unique<views::Throbber>()->GetMinimumSize(),
            make_icon()->GetMinimumSize());

  auto first_content_view = std::make_unique<PopupRowContentView>();
  popup_cell_utils::AddSuggestionContentToView(
      suggestion, make_main_label(), /*minor_text_label=*/nullptr,
      /*description_label=*/nullptr, /*subtext_views=*/{}, make_icon(),
      *first_content_view);

  suggestion.is_loading = Suggestion::IsLoading(true);
  auto second_content_view = std::make_unique<PopupRowContentView>();
  popup_cell_utils::AddSuggestionContentToView(
      suggestion, make_main_label(), /*minor_text_label=*/nullptr,
      /*description_label=*/nullptr, /*subtext_views=*/{}, make_icon(),
      *second_content_view);

  EXPECT_EQ(first_content_view->GetPreferredSize(),
            second_content_view->GetPreferredSize());
}

}  // namespace
}  // namespace autofill
