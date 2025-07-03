// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using testing::_;
using testing::Return;

std::u16string AllowedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL);
}

std::u16string SiteNotWorkingLabel() {
  return l10n_util::GetStringUTF16(
      IDS_TRACKING_PROTECTION_PAGE_ACTION_SITE_NOT_WORKING_LABEL);
}

std::u16string TrackingProtectionPausedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_TRACKING_PROTECTIONS_PAGE_ACTION_PROTECTIONS_PAUSED_LABEL);
}

std::u16string TrackingProtectionResumedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_TRACKING_PROTECTIONS_PAGE_ACTION_PROTECTIONS_RESUMED_LABEL);
}

std::u16string TrackingProtectionEnabledLabel() {
  return l10n_util::GetStringUTF16(
      IDS_TRACKING_PROTECTIONS_PAGE_ACTION_PROTECTIONS_ENABLED_LABEL);
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

// Mock implementation of the BubbleDelegate for testing.
class MockBubbleDelegate
    : public CookieControlsPageActionController::BubbleDelegate {
 public:
  MockBubbleDelegate() = default;
  ~MockBubbleDelegate() override = default;

  MOCK_METHOD(bool, IsReloading, (), (override));
  MOCK_METHOD(bool, HasBubble, (), (override));
};

class CookieControlsPageActionControllerTest
    : public testing::Test,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {
 public:
  CookieControlsPageActionControllerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageActionsMigration,
          {{features::kPageActionsMigrationCookieControls.name, "true"}}},
         {privacy_sandbox::kActUserBypassUx, {}}},
        {});

    auto mock_bubble_delegate = std::make_unique<MockBubbleDelegate>();
    mock_bubble_delegate_ = mock_bubble_delegate.get();

    cookie_controls_page_action_controller_ =
        std::make_unique<CookieControlsPageActionController>(
            mock_tab_interface_, page_action_controller_);
    controller().set_bubble_delegate_for_testing(
        std::move(mock_bubble_delegate));
  }

  void SetUp() override {
    // Default mock behavior. Tests can override these expectations.
    ON_CALL(*mock_bubble_delegate(), IsReloading()).WillByDefault(Return(true));
    ON_CALL(*mock_bubble_delegate(), HasBubble()).WillByDefault(Return(false));
  }

  CookieControlsPageActionController& controller() {
    return *cookie_controls_page_action_controller_;
  }

  FakePageActionController& page_action_controller() {
    return page_action_controller_;
  }

  MockBubbleDelegate* mock_bubble_delegate() { return mock_bubble_delegate_; }

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
  tabs::MockTabInterface mock_tab_interface_;
  FakePageActionController page_action_controller_;

  std::unique_ptr<CookieControlsPageActionController>
      cookie_controls_page_action_controller_;
  raw_ptr<MockBubbleDelegate> mock_bubble_delegate_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CookieControlsPageActionControllerTest,
                         testing::Values(CookieBlocking3pcdStatus::kNotIn3pcd,
                                         CookieBlocking3pcdStatus::kLimited,
                                         CookieBlocking3pcdStatus::kAll));

// New test: Verifies icon remains visible when the bubble is showing, even if
// the status says it should be hidden.
TEST_P(CookieControlsPageActionControllerTest, IconVisibleWhenBubbleShowing) {
  EXPECT_CALL(*mock_bubble_delegate(), HasBubble()).WillOnce(Return(true));
  // isReloading() is not checked if hasBubble() is true in this path.

  // The icon should be forced to show because the bubble is visible.
  EXPECT_CALL(page_action_controller(), Hide(kActionShowCookieControls))
      .Times(0);
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(1);

  // Call with icon_visible=false, which should be ignored.
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/false, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/false);
}

// New test: Verifies the suggestion chip is not shown when the bubble is open.
TEST_P(CookieControlsPageActionControllerTest, ChipNotShownWhenBubbleShowing) {
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading()).WillOnce(Return(true));
  EXPECT_CALL(*mock_bubble_delegate(), HasBubble()).WillOnce(Return(true));

  // The chip should NOT be shown because the bubble is already visible.
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(0);
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(1);

  // Call with should_highlight=true, which should be ignored for the chip.
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
}

// New test: Verifies that if the bubble is not in the "reloading" state, status
// updates are ignored.
TEST_P(CookieControlsPageActionControllerTest,
       StatusChangedIgnoredWhenNotReloading) {
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading()).WillOnce(Return(false));

  // No calls to the page action controller are expected.
  EXPECT_CALL(page_action_controller(), Show(_)).Times(0);
  EXPECT_CALL(page_action_controller(), Hide(_)).Times(0);
  EXPECT_CALL(page_action_controller(), ShowSuggestionChip(_, _)).Times(0);

  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
}

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimatesWhenShouldHighlightIsTrueAnd3pcsBlocked) {
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading()).WillOnce(Return(true));
  EXPECT_CALL(*mock_bubble_delegate(), HasBubble()).WillOnce(Return(false));
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
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_bubble_delegate(), HasBubble())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(2);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(2);

  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(),
            In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());

  // Invoking again should not change anything.
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(),
            In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
}

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimationTextUpdatesWhen3pcStateChanges) {
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_bubble_delegate(), HasBubble())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(2);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(1);

  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(),
            In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());

  // Invoking again with a new controls state should update the label.
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(), AllowedLabel());
}

TEST_P(CookieControlsPageActionControllerTest,
       IconDoesNotAnimateWhenShouldHighlightIsFalse) {
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading()).WillOnce(Return(true));
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
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading()).WillOnce(Return(true));
  EXPECT_CALL(*mock_bubble_delegate(), HasBubble()).WillOnce(Return(false));
  EXPECT_CALL(page_action_controller(), Hide(kActionShowCookieControls))
      .Times(1);
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls))
      .Times(0);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/false, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/false);
}

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimatesOnPageReloadWithChanged3pcSettings) {
  // Set initial state without highlighting.
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading()).WillOnce(Return(true));
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls));
  EXPECT_CALL(page_action_controller(), ShowSuggestionChip(_, _)).Times(0);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/false);
  testing::Mock::VerifyAndClearExpectations(&page_action_controller());

  // Force the icon to animate and set the label again upon reload.
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _));
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionShowCookieControls, BlockedLabel()));
  controller().OnFinishedPageReloadWithChangedSettings();

  // The label for the chip should be the "Blocked" label.
  EXPECT_EQ(page_action_controller().last_text(), BlockedLabel());
}

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimatesOnPageReloadWithChangedTpSettings) {
  EXPECT_CALL(*mock_bubble_delegate(), IsReloading())
      .WillRepeatedly(Return(true));

  // Default state when tracking protections are active.
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls));
  EXPECT_CALL(page_action_controller(), ShowSuggestionChip(_, _)).Times(0);
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionShowCookieControls,
                              TrackingProtectionEnabledLabel()));
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kActiveTp, GetParam(),
      /*should_highlight=*/false);
  // The icon is visible, but not animating, and has the correct tooltip.
  testing::Mock::VerifyAndClearExpectations(&page_action_controller());

  // When tracking protections are paused, the label is shown and updated.
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kPausedTp, GetParam(),
      /*should_highlight=*/false);
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls));
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _));
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionShowCookieControls,
                              TrackingProtectionPausedLabel()));
  controller().OnFinishedPageReloadWithChangedSettings();
  EXPECT_EQ(page_action_controller().last_text(),
            TrackingProtectionPausedLabel());
  testing::Mock::VerifyAndClearExpectations(&page_action_controller());

  // When tracking protections are resumed, the label is shown and updated.
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kActiveTp, GetParam(),
      /*should_highlight=*/false);

  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls));
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _));
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionShowCookieControls,
                              TrackingProtectionResumedLabel()));
  controller().OnFinishedPageReloadWithChangedSettings();
  EXPECT_EQ(page_action_controller().last_text(),
            TrackingProtectionResumedLabel());
}

}  // namespace
