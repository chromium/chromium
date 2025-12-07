// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_controller.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using testing::_;
using testing::Return;
using testing::ReturnRef;

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

// Fake implementation of the BubbleDelegate for testing.
class FakeBubbleDelegate
    : public CookieControlsPageActionController::BubbleDelegate {
 public:
  FakeBubbleDelegate() = default;
  ~FakeBubbleDelegate() override = default;

  MOCK_METHOD(bool, HasBubble, (), (override));
  MOCK_METHOD(void,
              ShowBubble,
              (ToolbarButtonProvider * toolbar_button_provider,
               content::WebContents* web_contents,
               content_settings::CookieControlsController* controller),
              (override));

  base::CallbackListSubscription RegisterBubbleClosingCallback(
      base::RepeatingClosure callback) override {
    return closing_callbacks_.Add(std::move(callback));
  }

  void TriggerBubbleClosed() { closing_callbacks_.Notify(); }

 private:
  base::RepeatingClosureList closing_callbacks_;
};

class CookieControlsPageActionControllerTestBase : public testing::Test {
 public:
  CookieControlsPageActionControllerTestBase() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageActionsMigration,
          {{features::kPageActionsMigrationCookieControls.name, "true"}}}},
        {});

    auto fake_bubble_delegate = std::make_unique<FakeBubbleDelegate>();
    fake_bubble_delegate_ = fake_bubble_delegate.get();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ON_CALL(mock_tab_interface_, GetContents())
        .WillByDefault(Return(web_contents_.get()));
    ON_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_interface_));
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    user_education_.emplace(&mock_browser_window_interface_);

    ON_CALL(page_action_controller_, AddActivity(kActionShowCookieControls))
        .WillByDefault([&](actions::ActionId) {
          return page_actions::ScopedPageActionActivity(
              page_action_controller_, kActionShowCookieControls);
        });

    ON_CALL(*user_education_, MaybeShowFeaturePromo(_))
        .WillByDefault([](user_education::FeaturePromoParams params) {
          std::move(params.show_promo_result_callback)
              .Run(user_education::FeaturePromoResult::Success());
        });

    ON_CALL(*fake_bubble_delegate_, HasBubble()).WillByDefault(Return(false));

    cookie_controls_page_action_controller_ =
        std::make_unique<CookieControlsPageActionController>(
            mock_tab_interface_, profile_, page_action_controller_);
    controller().set_bubble_delegate_for_testing(
        std::move(fake_bubble_delegate));

    controller().Init();
  }

  CookieControlsPageActionController& controller() {
    return *cookie_controls_page_action_controller_;
  }

  FakePageActionController& page_action_controller() {
    return page_action_controller_;
  }

  MockBrowserUserEducationInterface& user_education() {
    return user_education_.value();
  }

  TestingProfile& profile() { return profile_; }

  FakeBubbleDelegate* fake_bubble_delegate() { return fake_bubble_delegate_; }

  base::UserActionTester& user_action_tester() { return user_action_tester_; }

  tabs::MockTabInterface& mock_tab_interface() { return mock_tab_interface_; }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  base::test::ScopedFeatureList scoped_feature_list_;
  ui::UnownedUserDataHost user_data_host_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  tabs::MockTabInterface mock_tab_interface_;

  std::optional<MockBrowserUserEducationInterface> user_education_;
  FakePageActionController page_action_controller_;
  std::unique_ptr<CookieControlsPageActionController>
      cookie_controls_page_action_controller_;
  raw_ptr<FakeBubbleDelegate> fake_bubble_delegate_;
  base::UserActionTester user_action_tester_;
};

class CookieControlsPageActionControllerTest
    : public CookieControlsPageActionControllerTestBase,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {
 public:
  using CookieControlsPageActionControllerTestBase::
      CookieControlsPageActionControllerTestBase;

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
};

INSTANTIATE_TEST_SUITE_P(All,
                         CookieControlsPageActionControllerTest,
                         testing::Values(CookieBlocking3pcdStatus::kNotIn3pcd,
                                         CookieBlocking3pcdStatus::kLimited,
                                         CookieBlocking3pcdStatus::kAll));

// Verifies icon remains visible when the bubble is showing, even if
// the status says it should be hidden.
TEST_P(CookieControlsPageActionControllerTest, IconVisibleWhenBubbleShowing) {
  EXPECT_CALL(*fake_bubble_delegate(), HasBubble()).WillOnce(Return(true));

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

// Verifies the suggestion chip is not shown when the bubble is open.
TEST_P(CookieControlsPageActionControllerTest, ChipNotShownWhenBubbleShowing) {
  if (!In3pcd()) {
    GTEST_SKIP() << "IPHs are used when not in 3pcd";
  }
  EXPECT_CALL(*fake_bubble_delegate(), HasBubble()).WillOnce(Return(true));

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

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimatesWhenShouldHighlightIsTrueAnd3pcsBlocked) {
  if (!In3pcd()) {
    GTEST_SKIP() << "IPHs are used when not in 3pcd";
  }
  EXPECT_CALL(*fake_bubble_delegate(), HasBubble()).WillOnce(Return(false));
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
  if (!In3pcd()) {
    GTEST_SKIP() << "IPHs are used when not in 3pcd";
  }
  EXPECT_CALL(*fake_bubble_delegate(), HasBubble())
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

  // Invoking again should not change anything.
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  EXPECT_EQ(page_action_controller().last_text(),
            In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
}

TEST_P(CookieControlsPageActionControllerTest,
       IconAnimationTextUpdatesWhen3pcStateChanges) {
  if (!In3pcd()) {
    GTEST_SKIP() << "IPHs are used when not in 3pcd";
  }
  EXPECT_CALL(*fake_bubble_delegate(), HasBubble())
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
  EXPECT_CALL(*fake_bubble_delegate(), HasBubble()).WillOnce(Return(false));
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

TEST_P(CookieControlsPageActionControllerTest, ShowChipOnIPHFailure) {
  EXPECT_CALL(user_education(), MaybeShowFeaturePromo)
      .WillOnce([](user_education::FeaturePromoParams params) {
        std::move(params.show_promo_result_callback)
            .Run(user_education::FeaturePromoResult::kError);
      });
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(1);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
      CookieBlocking3pcdStatus::kNotIn3pcd,
      /*should_highlight=*/true);
}

TEST_P(CookieControlsPageActionControllerTest, SetActivityOnIPHShown) {
  EXPECT_CALL(user_education(), MaybeShowFeaturePromo)
      .WillOnce([](user_education::FeaturePromoParams params) {
        std::move(params.show_promo_result_callback)
            .Run(user_education::FeaturePromoResult::Success());
      });
  EXPECT_CALL(page_action_controller(), AddActivity(kActionShowCookieControls))
      .Times(1)
      .WillOnce(Return(page_actions::ScopedPageActionActivity(
          page_action_controller(), kActionShowCookieControls)));
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
      CookieBlocking3pcdStatus::kNotIn3pcd,
      /*should_highlight=*/true);
}

TEST_P(CookieControlsPageActionControllerTest, NoChipWhenIPHActive) {
  EXPECT_CALL(user_education(),
              IsFeaturePromoActive(
                  testing::Ref(feature_engagement::kIPHCookieControlsFeature)))
      .WillOnce(Return(true));
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCookieControls, _))
      .Times(0);
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
      CookieBlocking3pcdStatus::kAll,
      /*should_highlight=*/true);
}

TEST_P(CookieControlsPageActionControllerTest, WebContentsChangeUpdatesIcon) {
  // Set initial state.
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls));
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionShowCookieControls, BlockedLabel()));
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/false);
  EXPECT_EQ(page_action_controller().last_text(), BlockedLabel());
  testing::Mock::VerifyAndClearExpectations(&page_action_controller());

  // Simulate switching to a new tab (new WebContents).
  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContentsTester::CreateTestWebContents(&profile(), nullptr);
  ON_CALL(mock_tab_interface(), GetContents())
      .WillByDefault(Return(web_contents2.get()));

  // Send an update for the new tab. The tooltip and label should update.
  EXPECT_CALL(page_action_controller(), Show(kActionShowCookieControls));
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionShowCookieControls, AllowedLabel()));
  controller().OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/false);
  EXPECT_EQ(page_action_controller().last_text(), AllowedLabel());
}

TEST_P(CookieControlsPageActionControllerTest, ChipHidesAfterTimeout) {
  EXPECT_CALL(page_action_controller(),
              HideSuggestionChip(kActionShowCookieControls))
      .Times(1);

  page_actions::PageActionState state;
  controller().OnPageActionChipShown(state);

  task_environment().FastForwardBy(base::Seconds(12));
}

TEST_P(CookieControlsPageActionControllerTest, IconRespondsToBubbleHidden) {
  EXPECT_CALL(page_action_controller(), Hide(kActionShowCookieControls))
      .Times(1);

  fake_bubble_delegate()->TriggerBubbleClosed();
}

struct ActionTestParams {
  CookieControlsState controls_state;
  std::string action_name;
};

class CookieControlsPageActionControllerActionTest
    : public CookieControlsPageActionControllerTestBase,
      public testing::WithParamInterface<ActionTestParams> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    CookieControlsPageActionControllerActionTest,
    testing::Values(
        ActionTestParams{CookieControlsState::kBlocked3pc,
                         "CookieControls.Bubble.CookiesBlocked.Opened"},
        ActionTestParams{CookieControlsState::kAllowed3pc,
                         "CookieControls.Bubble.CookiesAllowed.Opened"}));

TEST_P(CookieControlsPageActionControllerActionTest, ExecuteAction) {
  // Setup controller so that the icon is visible.
  controller().OnCookieControlsIconStatusChanged(
      true, GetParam().controls_state, CookieBlocking3pcdStatus::kNotIn3pcd,
      false);

  EXPECT_CALL(*fake_bubble_delegate(), ShowBubble);
  EXPECT_CALL(user_education(),
              NotifyFeaturePromoFeatureUsed(
                  testing::Ref(feature_engagement::kIPHCookieControlsFeature),
                  FeaturePromoFeatureUsedAction::kClosePromoIfPresent));
  controller().ExecutePageAction(nullptr);

  EXPECT_EQ(1, user_action_tester().GetActionCount(GetParam().action_name));
}
}  // namespace
