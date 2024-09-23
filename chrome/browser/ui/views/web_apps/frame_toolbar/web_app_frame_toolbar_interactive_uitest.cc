// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace {

// Param: DesktopPWAsElidedExtensionsMenu feature.
class WebAppFrameToolbarInteractiveUITest
    : public InteractiveBrowserTestT<extensions::ExtensionBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  WebAppFrameToolbarInteractiveUITest() {
    feature_list_.InitWithFeatureState(
        ::features::kDesktopPWAsElidedExtensionsMenu, IsExtensionsMenuElided());
  }

  void LoadAndLaunchExtension() {
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("simple_with_icon/")));

    const GURL app_url("https://test.org");
    helper()->InstallAndLaunchWebApp(browser(), app_url);
  }

  auto SetUpExtensionsContainer() {
    return Do([this]() {
      ExtensionsToolbarContainer* const extensions_container =
          helper()->web_app_frame_toolbar()->GetExtensionsToolbarContainer();
      views::test::ReduceAnimationDuration(extensions_container);
      views::test::WaitForAnimatingLayoutManager(extensions_container);
    });
  }

  auto SetToolbarFocusable() {
    return Do([this]() {
      helper()->browser_view()->GetFocusManager()->SetKeyboardAccessible(true);
    });
  }

  // Send focus to the toolbar as if the user pressed Alt+Shift+T.
  // TODO(b/319234054): Use Kombucha API SendAccelerator() once the keyboard
  // shortcuts for IDC_FOCUS_TOOLBAR is supported on Mac.
  auto FocusToolbar() {
    return Do([this]() {
      helper()->app_browser()->command_controller()->ExecuteCommand(
          IDC_FOCUS_TOOLBAR);
    });
  }

  auto CycleFocusForward() {
    return Do([this]() {
      helper()->browser_view()->GetFocusManager()->AdvanceFocus(false);
    });
  }

  // Simulate the user pressing Shift-Tab to cycle backwards.
  auto CycleFocusBackward() {
    return Do([this]() {
      helper()->browser_view()->GetFocusManager()->AdvanceFocus(true);
    });
  }

  auto CheckViewFocused(ElementSpecifier view) {
    return std::move(
        CheckView(
            view,
            [](views::View* view) {
              if (view->HasFocus()) {
                return true;
              }
              auto* const focused = view->GetFocusManager()->GetFocusedView();
              LOG(ERROR) << "Expected " << view->GetClassName()
                         << " to be focused, but focused view is "
                         << (focused ? focused->GetClassName() : "(none)");
              return false;
            },
            true)
            .SetDescription("CheckViewFocused()"));
  }

  auto VerifyExtensionsMenuButtonIfNeeded(bool go_forward) {
    if (IsExtensionsMenuElided()) {
      return Steps(Do([]() { base::DoNothing(); }));
    }

    if (go_forward) {
      return Steps(CycleFocusForward(),
                   CheckViewFocused(kExtensionsMenuButtonElementId));
    } else {
      return Steps(CycleFocusBackward(),
                   CheckViewFocused(kExtensionsMenuButtonElementId));
    }
  }

  ui::ElementContext GetAppWindowElementContext() {
    return helper()->app_browser()->window()->GetElementContext();
  }

 private:
  WebAppFrameToolbarTestHelper* helper() {
    return &web_app_frame_toolbar_helper_;
  }
  bool IsExtensionsMenuElided() const { return GetParam(); }

  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that for minimal-ui web apps, the toolbar keyboard focus cycles
// among the toolbar buttons: the reload button, the extensions menu button, and
// the app menu button, in that order.
IN_PROC_BROWSER_TEST_P(WebAppFrameToolbarInteractiveUITest, CycleFocusForward) {
  LoadAndLaunchExtension();

  RunTestSequenceInContext(
      GetAppWindowElementContext(), SetUpExtensionsContainer(),
#if BUILDFLAG(IS_MAC)
      // Mac doesn't have a focusable toolbar by default.
      SetToolbarFocusable(),
#endif
      FocusToolbar(), CheckViewFocused(kReloadButtonElementId),
      VerifyExtensionsMenuButtonIfNeeded(/*go_forward=*/true),
      CycleFocusForward(), CheckViewFocused(kToolbarAppMenuButtonElementId),
      CycleFocusForward(), CheckViewFocused(kReloadButtonElementId));
}

IN_PROC_BROWSER_TEST_P(WebAppFrameToolbarInteractiveUITest,
                       CycleFocusBackwards) {
  LoadAndLaunchExtension();

  RunTestSequenceInContext(
      GetAppWindowElementContext(), SetUpExtensionsContainer(),
#if BUILDFLAG(IS_MAC)
      // Mac doesn't have a focusable toolbar by default.
      SetToolbarFocusable(),
#endif
      FocusToolbar(), CheckViewFocused(kReloadButtonElementId),
      VerifyExtensionsMenuButtonIfNeeded(/*go_forward=*/true),
      CycleFocusForward(),
      CheckViewProperty(kToolbarAppMenuButtonElementId, &views::View::HasFocus,
                        true),
      CycleFocusForward(), CheckViewFocused(kReloadButtonElementId),
      CycleFocusBackward(),
      CheckViewProperty(kToolbarAppMenuButtonElementId, &views::View::HasFocus,
                        true),
      VerifyExtensionsMenuButtonIfNeeded(/*go_forward=*/false),
      CycleFocusBackward(), CheckViewFocused(kReloadButtonElementId));
}

IN_PROC_BROWSER_TEST_P(WebAppFrameToolbarInteractiveUITest,
                       NavigationShowsBackButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAppWindowId);
  LoadAndLaunchExtension();

  RunTestSequenceInContext(
      GetAppWindowElementContext(), SetUpExtensionsContainer(),
      InstrumentTab(kAppWindowId),
      NavigateWebContents(kAppWindowId, GURL("https://anothertest.org")),
#if BUILDFLAG(IS_MAC)
      // Mac doesn't have a focusable toolbar by default.
      SetToolbarFocusable(),
#endif
      FocusToolbar(),
      CheckViewProperty(kToolbarBackButtonElementId, &views::View::HasFocus,
                        true));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppFrameToolbarInteractiveUITest,
                         ::testing::Bool());

}  // namespace
