// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace base::test {

class FakeImmersiveModeController : public ImmersiveModeController {
 public:
  explicit FakeImmersiveModeController(ui::UnownedUserDataHost& host)
      : ImmersiveModeController(host) {}
  ~FakeImmersiveModeController() override = default;

  void Init(BrowserView* browser_view) override {
    browser_view_ = browser_view;
  }
  void SetEnabled(bool enabled) override {
    enabled_ = enabled;
#if BUILDFLAG(IS_MAC)
    if (browser_view_ && browser_view_->overlay_widget()) {
      if (enabled_) {
        browser_view_->overlay_widget()->Show();
      } else {
        browser_view_->overlay_widget()->Hide();
      }
    }
#endif
    if (enabled_) {
      for (Observer& observer : observers_) {
        observer.OnImmersiveFullscreenEntered();
      }
    } else {
      for (Observer& observer : observers_) {
        observer.OnImmersiveFullscreenExited();
      }
    }
  }
  bool IsEnabled() const override { return enabled_; }
  bool IsRevealed() const override { return false; }
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override {
    return 0;
  }
  std::unique_ptr<ImmersiveRevealedLock> GetRevealedLock(
      AnimateReveal animate_reveal) override {
    return nullptr;
  }
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) override {}
  bool ShouldStayImmersiveAfterExitingFullscreen() override { return true; }
  int GetMinimumContentOffset() const override { return 0; }
  int GetExtraInfobarOffset() const override { return 0; }
  void OnContentFullscreenChanged(bool is_content_fullscreen) override {}
  void AddObserver(Observer* observer) override {
    ImmersiveModeController::AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    ImmersiveModeController::RemoveObserver(observer);
  }

 private:
  raw_ptr<BrowserView> browser_view_ = nullptr;
  bool enabled_ = false;
};

class VerticalTabStripInteractiveUiTest : public InteractiveBrowserTest {
 public:
  VerticalTabStripInteractiveUiTest() = default;
  ~VerticalTabStripInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {tabs::kVerticalTabs,
                                tabs::kVerticalTabsExpandOnHover},
        /* disabled_features */ {});
    override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting<FakeImmersiveModeController>(
                base::BindRepeating(
                    &VerticalTabStripInteractiveUiTest::CreateFakeController,
                    base::Unretained(this)));
    InteractiveBrowserTest::SetUp();
  }

  bool SystemMenuContainsStringId(int message_id) {
    ui::MenuModel* menu_model =
        browser()->GetBrowserView().browser_widget()->GetSystemMenuModel();
    for (size_t i = 0; i < menu_model->GetItemCount(); i++) {
      if (l10n_util::GetStringUTF16(message_id) == menu_model->GetLabelAt(i)) {
        return true;
      }
    }
    return false;
  }

  void PostRunTestOnMainThread() override {
    fake_controller_ = nullptr;
    InteractiveBrowserTest::PostRunTestOnMainThread();
  }

 protected:
  std::unique_ptr<FakeImmersiveModeController> CreateFakeController(
      BrowserWindowInterface& owner) {
    auto controller = std::make_unique<FakeImmersiveModeController>(
        owner.GetUnownedUserDataHost());
    fake_controller_ = controller.get();
    return controller;
  }

  raw_ptr<FakeImmersiveModeController> fake_controller_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<ui::UserDataFactory::ScopedOverride> override_;
};

// Unable to programmatically click System Context Menu Items in Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_VerifyTabsToTheSideButton DISABLED_VerifyTabsToTheSideButton
#else
#define MAYBE_VerifyTabsToTheSideButton VerifyTabsToTheSideButton
#endif
// This test checks that we can click the show tabs to the side button
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_VerifyTabsToTheSideButton) {
  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_VERTICAL_TAB));

  RunTestSequence(
      WaitForShow(kTabStripFrameGrabHandleElementId),
      EnsurePresent(kTabStripFrameGrabHandleElementId),
      MoveMouseTo(kTabStripFrameGrabHandleElementId),
      ClickMouse(ui_controls::RIGHT),
      WaitForShow(SystemMenuModelBuilder::kToggleVerticalTabsElementId),
      SelectMenuItem(SystemMenuModelBuilder::kToggleVerticalTabsElementId),
      WaitForShow(kVerticalTabStripCollapseButtonElementId));

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_HORIZONTAL_TAB));
}

// Unable to programmatically click System Context Menu Items in Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_VerifyTabsToTheTopButton DISABLED_VerifyTabsToTheTopButton
#else
#define MAYBE_VerifyTabsToTheTopButton VerifyTabsToTheTopButton
#endif
// This test checks that we can click the show tabs at the top button
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_VerifyTabsToTheTopButton) {
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_HORIZONTAL_TAB));

  RunScheduledLayouts();

  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripTopContainerElementId),
      MoveMouseTo(kVerticalTabStripTopContainerElementId),
      ClickMouse(ui_controls::RIGHT),
      WaitForShow(SystemMenuModelBuilder::kToggleVerticalTabsElementId),
      SelectMenuItem(SystemMenuModelBuilder::kToggleVerticalTabsElementId),
      WaitForShow(kTabStripFrameGrabHandleElementId));

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_VERTICAL_TAB));
}

// Unable to programmatically click System Context Menu Items in Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_EnablingExpandOnHoverSystemContextMenu \
  DISABLED_EnablingExpandOnHoverSystemContextMenu
#else
#define MAYBE_EnablingExpandOnHoverSystemContextMenu \
  EnablingExpandOnHoverSystemContextMenu
#endif
// This test checks that we can enable the expand on hover behavior via the
// system context menu.
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_EnablingExpandOnHoverSystemContextMenu) {
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  tabs::VerticalTabStripStateController::From(browser())
      ->SetExpandOnHoverEnabled(false);

  EXPECT_TRUE(
      SystemMenuContainsStringId(IDS_VERTICAL_TABS_ENABLE_EXPAND_ON_HOVER));

  RunScheduledLayouts();

  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripTopContainerElementId),
      MoveMouseTo(kVerticalTabStripTopContainerElementId),
      ClickMouse(ui_controls::RIGHT),
      WaitForShow(
          SystemMenuModelBuilder::kToggleVerticalTabsExpandOnHoverElementId),
      SelectMenuItem(
          SystemMenuModelBuilder::kToggleVerticalTabsExpandOnHoverElementId));

  EXPECT_TRUE(
      SystemMenuContainsStringId(IDS_VERTICAL_TABS_DISABLE_EXPAND_ON_HOVER));
}

// Unable to programmatically click System Context Menu Items in Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DisablingExpandOnHoverSystemContextMenu \
  DISABLED_DisablingExpandOnHoverSystemContextMenu
#else
#define MAYBE_DisablingExpandOnHoverSystemContextMenu \
  DisablingExpandOnHoverSystemContextMenu
#endif
// This test checks that we can disable the expand on hover behavior via the
// system context menu.
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_DisablingExpandOnHoverSystemContextMenu) {
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);

  EXPECT_TRUE(
      SystemMenuContainsStringId(IDS_VERTICAL_TABS_DISABLE_EXPAND_ON_HOVER));

  RunScheduledLayouts();

  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripTopContainerElementId),
      MoveMouseTo(kVerticalTabStripTopContainerElementId),
      ClickMouse(ui_controls::RIGHT),
      WaitForShow(
          SystemMenuModelBuilder::kToggleVerticalTabsExpandOnHoverElementId),
      SelectMenuItem(
          SystemMenuModelBuilder::kToggleVerticalTabsExpandOnHoverElementId));

  EXPECT_TRUE(
      SystemMenuContainsStringId(IDS_VERTICAL_TABS_ENABLE_EXPAND_ON_HOVER));
}

struct VerticalTabsBadgeTestParams {
  base::test::FeatureRef testing_feature;
  ui::NewBadgeType expected_badge_type;
};

class VerticalTabStripMenuInteractiveUiTest
    : public ::testing::WithParamInterface<VerticalTabsBadgeTestParams>,
      public InteractiveFeaturePromoTest {
 public:
  VerticalTabStripMenuInteractiveUiTest()
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos({GetParam().testing_feature})) {}
  ~VerticalTabStripMenuInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
    InteractiveFeaturePromoTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(VerticalTabStripMenuInteractiveUiTest,
                       ShowBadgeInContextMenuToggle) {
  BrowserWidget* const browser_widget =
      BrowserView::GetBrowserViewForBrowser(browser())->browser_widget();
  ui::MenuModel* menu = browser_widget->GetSystemMenuModel();
  size_t command_index = 0u;
  ui::MenuModel::GetModelAndIndexForCommandId(IDC_TOGGLE_VERTICAL_TABS, &menu,
                                              &command_index);
  tabs::VerticalTabStripStateController* const vertical_tabs_controller =
      tabs::VerticalTabStripStateController::From(browser());
  ASSERT_FALSE(vertical_tabs_controller->ShouldDisplayVerticalTabs());

  // The badge should show while using the horizontal tab strip.
  std::optional<ui::NewBadgeType> badge_type =
      menu->GetNewBadgeTypeAt(command_index);
  ASSERT_TRUE(badge_type.has_value());
  EXPECT_EQ(badge_type.value(), GetParam().expected_badge_type);

  // While using the vertical tab strip, the badge should be hidden.
  vertical_tabs_controller->SetVerticalTabsEnabled(true);
  menu = browser_widget->GetSystemMenuModel();
  std::optional<ui::NewBadgeType> badge_type_in_vertical_tabs =
      menu->GetNewBadgeTypeAt(command_index);
  EXPECT_FALSE(badge_type_in_vertical_tabs.has_value());

  // Switching back to the horizontal tab strip should show the badge
  // again.
  vertical_tabs_controller->SetVerticalTabsEnabled(false);
  menu = browser_widget->GetSystemMenuModel();
  std::optional<ui::NewBadgeType> badge_type_in_horizontal_tabs =
      menu->GetNewBadgeTypeAt(command_index);
  ASSERT_TRUE(badge_type_in_horizontal_tabs.has_value());
  EXPECT_EQ(badge_type_in_horizontal_tabs.value(),
            GetParam().expected_badge_type);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VerticalTabStripMenuInteractiveUiTest,
    ::testing::Values(
        VerticalTabsBadgeTestParams{
            .testing_feature = tabs::kVerticalTabsPreviewBadge,
            .expected_badge_type = ui::NewBadgeType::kPreview},
        VerticalTabsBadgeTestParams{
            .testing_feature = tabs::kVerticalTabsNewBadge,
            .expected_badge_type = ui::NewBadgeType::kNew}),
    [](const ::testing::TestParamInfo<
        VerticalTabStripMenuInteractiveUiTest::ParamType>& info) {
      return info.param.expected_badge_type == ui::NewBadgeType::kPreview
                 ? "PreviewBadge"
                 : "NewBadge";
    });

IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       ImmersiveFullscreenSwitchShowToast) {
  // Enter immersive fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_TRUE(browser()->GetWindow()->IsFullscreen());
  fake_controller_->SetEnabled(true);

  // Get ToastController
  ToastController* const toast_controller =
      browser()->browser_window_features()->toast_controller();
  ASSERT_NE(toast_controller, nullptr);
  EXPECT_FALSE(toast_controller->IsShowingToast());

  // Try to enable vertical tabs
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);

  // Stop the timer so it doesn't auto-dismiss during test execution.
  toast_controller->GetToastCloseTimerForTesting()->Stop();

  // Verify that vertical tabs are NOT enabled because we are in immersive
  // fullscreen (state is locked)
  EXPECT_FALSE(tabs::VerticalTabStripStateController::From(browser())
                   ->ShouldDisplayVerticalTabs());

  // Verify that toast is showing and has the correct ID
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(),
            ToastId::kTabStripSwitchDelayedVertical);

  // Click the action button on the toast to exit fullscreen.
  RunTestSequence(
      WaitForShow(toasts::ToastView::kToastActionButton), Do([]() {
        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, run_loop.QuitClosure(), base::Seconds(20));
        run_loop.Run();
      }),
      PressButton(toasts::ToastView::kToastActionButton),
      WaitForHide(toasts::ToastView::kToastViewId));

  // Verify we exited fullscreen and vertical tabs are now enabled!
  fake_controller_->SetEnabled(false);
  EXPECT_FALSE(browser()->GetWindow()->IsFullscreen());
  EXPECT_TRUE(tabs::VerticalTabStripStateController::From(browser())
                  ->ShouldDisplayVerticalTabs());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       ImmersiveFullscreenSwitchShowHorizontalToast) {
  // Enable vertical tabs first
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  ASSERT_TRUE(tabs::VerticalTabStripStateController::From(browser())
                  ->ShouldDisplayVerticalTabs());

  // Enter immersive fullscreen
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_TRUE(browser()->GetWindow()->IsFullscreen());
  fake_controller_->SetEnabled(true);

  // Get ToastController
  ToastController* const toast_controller =
      browser()->browser_window_features()->toast_controller();
  ASSERT_NE(toast_controller, nullptr);
  EXPECT_FALSE(toast_controller->IsShowingToast());

  // Try to disable vertical tabs
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(false);

  // Stop the timer so it doesn't auto-dismiss during test execution.
  toast_controller->GetToastCloseTimerForTesting()->Stop();

  // Verify that vertical tabs are STILL enabled because state is locked
  EXPECT_TRUE(tabs::VerticalTabStripStateController::From(browser())
                  ->ShouldDisplayVerticalTabs());

  // Verify that horizontal toast is showing
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(),
            ToastId::kTabStripSwitchDelayedHorizontal);

  // Click action button to exit fullscreen
  RunTestSequence(
      WaitForShow(toasts::ToastView::kToastActionButton), Do([]() {
        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, run_loop.QuitClosure(), base::Seconds(20));
        run_loop.Run();
      }),
      PressButton(toasts::ToastView::kToastActionButton),
      WaitForHide(toasts::ToastView::kToastViewId));

  // Verify we exited fullscreen and vertical tabs are now disabled!
  fake_controller_->SetEnabled(false);
  EXPECT_FALSE(browser()->GetWindow()->IsFullscreen());
  EXPECT_FALSE(tabs::VerticalTabStripStateController::From(browser())
                   ->ShouldDisplayVerticalTabs());
}

}  // namespace base::test
