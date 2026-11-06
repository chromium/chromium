// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_glic_actor_task_icon_control.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace {

using toolbar_ui_api::mojom::GlicActorTaskIconState;

auto StateMatches(bool should_show,
                  std::optional<std::string> nudge_label,
                  const std::string& tooltip,
                  bool is_pressed) {
  return testing::Pointee(testing::AllOf(
      testing::Field("should_show", &GlicActorTaskIconState::should_show,
                     should_show),
      testing::Field("nudge_label", &GlicActorTaskIconState::nudge_label,
                     nudge_label),
      testing::Field("tooltip", &GlicActorTaskIconState::tooltip, tooltip),
      testing::Field("is_pressed", &GlicActorTaskIconState::is_pressed,
                     is_pressed)));
}

std::string DefaultTooltip() {
  return l10n_util::GetStringUTF8(IDS_ACTOR_TASK_INDICATOR_TOOLTIP);
}

}  // namespace

class WebUIGlicActorTaskIconControlTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();

    browser_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    ON_CALL(*browser_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    delegate_ =
        std::make_unique<testing::NiceMock<MockWebUIToolbarControlDelegate>>();
    ON_CALL(*delegate_, GetBrowser())
        .WillByDefault(testing::Return(browser_interface_.get()));
    ON_CALL(*delegate_, GetView())
        .WillByDefault(testing::Return(&anchor_view_));

    control_ = std::make_unique<WebUIGlicActorTaskIconControl>(delegate_.get());
  }

  void TearDown() override {
    control_.reset();
    delegate_.reset();
    browser_interface_.reset();
    profile_.reset();
    testing::Test::TearDown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  views::View anchor_view_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_interface_;
  std::unique_ptr<testing::NiceMock<MockWebUIToolbarControlDelegate>> delegate_;
  std::unique_ptr<WebUIGlicActorTaskIconControl> control_;
};

TEST_F(WebUIGlicActorTaskIconControlTest, InitPushesDefaultState) {
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/false, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  control_->Init();
}

TEST_F(WebUIGlicActorTaskIconControlTest, NoStateUpdatesBeforeInit) {
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  control_->SetVisible(true);
  control_->SetPressedState(true);
  EXPECT_TRUE(control_->IsVisible());
  EXPECT_TRUE(control_->GetIsPressed());
}

TEST_F(WebUIGlicActorTaskIconControlTest, SetVisible) {
  control_->Init();

  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/true, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged());
  control_->SetVisible(true);
  EXPECT_TRUE(control_->IsVisible());
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // No-op when unchanged.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(0);
  control_->SetVisible(true);
}

TEST_F(WebUIGlicActorTaskIconControlTest,
       NudgeLifecycleRequiresIsShowingNudgeAndClearsOnHide) {
  control_->Init();
  control_->SetVisible(true);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // ToolbarView::ShowGlicActorNudge() stages `ShowNudgeLabel()` before
  // `ShowToolbarNudge()` (which may defer `SetIsShowingNudge(true)` while the
  // toolbar is hovered). Staged text is not pushed until the nudge is shown.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  control_->ShowNudgeLabel(u"Needs review");
  EXPECT_FALSE(control_->GetIsShowingNudge());
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // Showing the nudge publishes the label and overrides the tooltip.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/true, "Needs review",
                              "Needs review", /*is_pressed=*/false)));
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(0);
  control_->SetIsShowingNudge(true);
  EXPECT_TRUE(control_->GetIsShowingNudge());
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // Updating the label while already showing updates immediately.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/true, "2 tasks complete",
                              "2 tasks complete", /*is_pressed=*/false)));
  control_->ShowNudgeLabel(u"2 tasks complete");
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // Setting the same label again is a no-op.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  control_->ShowNudgeLabel(u"2 tasks complete");
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // ToolbarView::ShowGlicActorTaskIcon() hides an active nudge via
  // `ExecuteHideToolbarNudge()` -> `SetIsShowingNudge(false)` without calling
  // `SetTaskIconToDefault()`, which must clear the nudge label and restore the
  // default tooltip while keeping the icon visible.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/true, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  control_->SetIsShowingNudge(false);
  EXPECT_FALSE(control_->GetIsShowingNudge());
}

TEST_F(WebUIGlicActorTaskIconControlTest,
       SetTaskIconToDefaultClearsActiveNudge) {
  control_->Init();
  control_->SetVisible(true);
  control_->ShowNudgeLabel(u"Needs review");
  control_->SetIsShowingNudge(true);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/true, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  control_->SetTaskIconToDefault();
  EXPECT_FALSE(control_->GetIsShowingNudge());
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // Resetting again is a no-op.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  control_->SetTaskIconToDefault();
}

TEST_F(WebUIGlicActorTaskIconControlTest, SetVisibleFalseClearsNudgeState) {
  control_->Init();
  control_->SetVisible(true);
  control_->ShowNudgeLabel(u"Needs review");
  control_->SetIsShowingNudge(true);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/false, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged());
  control_->SetVisible(false);
  EXPECT_FALSE(control_->GetIsShowingNudge());
}

TEST_F(WebUIGlicActorTaskIconControlTest, PressedState) {
  control_->Init();
  control_->SetVisible(true);

  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/true, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/true)));
  control_->SetPressedState(true);
  EXPECT_TRUE(control_->GetIsPressed());
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // Setting the same pressed state again is a no-op.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  control_->SetPressedState(true);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/true, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  control_->SetPressedState(false);
  EXPECT_FALSE(control_->GetIsPressed());
}

TEST_F(WebUIGlicActorTaskIconControlTest,
       OnClickedWithNoActivityManagerIsNoOp) {
  base::UserActionTester user_action_tester;
  control_->Init();
  control_->SetVisible(true);
  control_->OnClicked();
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.Default.Click"));
}

TEST_F(WebUIGlicActorTaskIconControlTest, OnClickedBeforeInitIsNoOp) {
  EXPECT_CALL(*delegate_, GetBrowser()).Times(0);
  control_->OnClicked();
}

TEST_F(WebUIGlicActorTaskIconControlTest, OnClickedWhenHiddenIsNoOp) {
  EXPECT_CALL(*delegate_, GetBrowser()).Times(0);
  control_->Init();
  control_->OnClicked();
}

TEST_F(WebUIGlicActorTaskIconControlTest,
       SetIsShowingNudgeTrueIgnoredWhenHiddenOrLabelEmpty) {
  control_->Init();
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // Hidden control ignores SetIsShowingNudge(true) even if a label was staged
  // before hiding while hover-locked.
  control_->SetVisible(true);
  control_->ShowNudgeLabel(u"Needs review");
  control_->SetVisible(false);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  control_->SetIsShowingNudge(true);
  EXPECT_FALSE(control_->GetIsShowingNudge());

  // Visible control with empty label also ignores SetIsShowingNudge(true).
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(1);
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(1);
  control_->SetVisible(true);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  control_->SetIsShowingNudge(true);
  EXPECT_FALSE(control_->GetIsShowingNudge());
}

TEST_F(WebUIGlicActorTaskIconControlTest, SetCanShowFalseHidesActorTaskIcon) {
  control_->Init();
  control_->SetVisible(true);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // SetCanShow(false) hides the icon without changing its own visibility.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/false, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged());
  control_->SetCanShow(false);
  EXPECT_FALSE(control_->ShouldShow());
  EXPECT_TRUE(control_->IsVisible());
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // Repeating the same value is a no-op.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged).Times(0);
  control_->SetCanShow(false);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // SetCanShow(true) restores the icon.
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/true, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged());
  control_->SetCanShow(true);
  EXPECT_TRUE(control_->ShouldShow());
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // SetCanShow(true) does not show an icon that is not visible itself.
  control_->SetVisible(false);
  control_->SetCanShow(false);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());
  EXPECT_CALL(*delegate_, OnGlicActorTaskIconStateChanged(StateMatches(
                              /*should_show=*/false, std::nullopt,
                              DefaultTooltip(), /*is_pressed=*/false)));
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(0);
  control_->SetCanShow(true);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());

  // SetVisible() does not change the preferred size while the icon cannot
  // show.
  control_->SetCanShow(false);
  testing::Mock::VerifyAndClearExpectations(delegate_.get());
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(0);
  control_->SetVisible(true);
  EXPECT_FALSE(control_->ShouldShow());
}
