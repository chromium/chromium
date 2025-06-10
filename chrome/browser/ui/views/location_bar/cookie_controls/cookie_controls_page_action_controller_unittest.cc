// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using testing::_;

std::u16string AllowedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL);
}

std::u16string SiteNotWorkingLabel() {
  return l10n_util::GetStringUTF16(
      IDS_TRACKING_PROTECTION_PAGE_ACTION_SITE_NOT_WORKING_LABEL);
}

class FakePageActionController : public page_actions::MockPageActionController {
 public:
  FakePageActionController() = default;
  ~FakePageActionController() override = default;

  void OverrideText(actions::ActionId action_id,
                    const std::u16string& text) override {
    page_actions::MockPageActionController::OverrideText(action_id, text);
    last_text_ = text;
  }

  void ClearOverrideText(actions::ActionId action_id) override {
    page_actions::MockPageActionController::ClearOverrideText(action_id);
    last_text_ = u"";
  }

  const std::u16string& last_text() const { return last_text_; }

 private:
  std::u16string last_text_;
};

class CookieControlsPageActionControllerTest
    : public testing::Test,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {
 public:
  CookieControlsPageActionControllerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageActionsMigration,
        {{features::kPageActionsMigrationCookieControls.name, "true"}});

    cookie_controls_page_action_controller_ =
        std::make_unique<CookieControlsPageActionController>(
            page_action_controller_);
  }

  CookieControlsPageActionController& controller() {
    return *cookie_controls_page_action_controller_;
  }

  FakePageActionController& page_action_controller() {
    return page_action_controller_;
  }

  bool In3pcd() const {
    return GetParam() != CookieBlocking3pcdStatus::kNotIn3pcd;
  }

  std::u16string BlockedLabel() const {
    return GetParam() == CookieBlocking3pcdStatus::kLimited
               ? l10n_util::GetStringUTF16(
                     IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_LIMITED_LABEL)
               : l10n_util::GetStringUTF16(
                     IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakePageActionController page_action_controller_;

  std::unique_ptr<CookieControlsPageActionController>
      cookie_controls_page_action_controller_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CookieControlsPageActionControllerTest,
                         testing::Values(CookieBlocking3pcdStatus::kNotIn3pcd,
                                         CookieBlocking3pcdStatus::kLimited,
                                         CookieBlocking3pcdStatus::kAll));

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimatesWhenShouldHighlightIsTrueAnd3pcsBlocked) {
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionShowCookieControls, BlockedLabel()))
      .Times(1);

  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);

  EXPECT_EQ(page_action_controller().last_text(),
            In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
}

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimationTextDoesNotResetWhenStateDoesNotChange) {
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(1);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(),
            In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());

  // Invoking again should not change anything.
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(1);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(),
            In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
}

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimationTextUpdatesWhen3pcStateChanges) {
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(1);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(),
            In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());

  // Invoking again with a new controls state should update the label.
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(1);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(), AllowedLabel());
}

TEST_P(CookieControlsPageActionControllerTest,
       IconDoesNotAnimateWhenShouldHighlightIsFalse) {
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(0);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/false);
}

TEST_P(CookieControlsPageActionControllerTest,
       IconHiddenWhenIconVisibleIsFalse) {
  EXPECT_CALL(page_action_controller(), Hide(kActionShowCookieControls))
      .Times(1);
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(0);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/false, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/false);
}

}  // namespace
