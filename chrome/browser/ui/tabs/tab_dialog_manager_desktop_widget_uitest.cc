// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/interaction/view_focus_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace tabs {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDialogViewId);

class TabDialogManagerDesktopWidgetUiTest : public InteractiveBrowserTest {
 public:
  TabDialogManagerDesktopWidgetUiTest() {
    feature_list_.InitAndEnableFeature(features::kTabModalUsesDesktopWidget);
  }

  TabDialogManagerDesktopWidgetUiTest(
      const TabDialogManagerDesktopWidgetUiTest&) = delete;
  TabDialogManagerDesktopWidgetUiTest& operator=(
      const TabDialogManagerDesktopWidgetUiTest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    // Activation fails on Weston but passes on Mutter, but we don't have
    // a way to detect which backend we're using.
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif
  }

 protected:
  std::unique_ptr<views::Widget> CreateAndShowTestDialog() {
    ui::DialogModel::Builder dialog_builder;
    dialog_builder.SetInternalName("TestDialog");
    dialog_builder.AddParagraph(ui::DialogModelLabel(u"Test"), u"",
                                kDialogViewId);

    TabDialogManager* manager = GetTabDialogManager();
    CHECK(manager);

    auto model_host = views::BubbleDialogModelHost::CreateModal(
        dialog_builder.Build(), ui::mojom::ModalType::kChild);
    model_host->SetOwnershipOfNewWidget(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    return manager->CreateAndShowDialog(
        model_host.release(),
        std::make_unique<tabs::TabDialogManager::Params>());
  }

  TabDialogManager* GetTabDialogManager() {
    TabInterface* tab_interface = browser()->GetActiveTabInterface();
    CHECK(tab_interface);
    return tab_interface->GetTabFeatures()->tab_dialog_manager();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that when TabModalUsesDesktopWidget is enabled, the created widget
// is a desktop widget.
IN_PROC_BROWSER_TEST_F(TabDialogManagerDesktopWidgetUiTest,
                       CreatesDesktopWidget) {
  auto widget = CreateAndShowTestDialog();
  EXPECT_TRUE(widget->GetIsDesktopWidget());
}

// TODO(crbug.com/430291260): macOS does not forward the focus to the dialog
// when the contents views::WebView is focused.
// TODO(crbug.com/431143409): widget activation does not work on Wayland.
#if !BUILDFLAG(IS_MAC)
// Tests that the modal dialog is activated when the contents views::WebView
// is focused.
IN_PROC_BROWSER_TEST_F(TabDialogManagerDesktopWidgetUiTest,
                       FocusWebViewWhenModalIsShowing) {
  std::unique_ptr<views::Widget> widget;

  RunTestSequence(
      ObserveState(views::test::kCurrentWidgetFocus),
      ObserveState(
          views::test::kCurrentFocusedViewId,
          BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()),
      // Click on the omnibox and check that it has focus (Omnibox is a random
      // choice, the focus can be on anything as long as it is not the contents
      // views::WebView).
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      WaitForState(views::test::kCurrentFocusedViewId, kOmniboxElementId),
      // Show a dialog.
      Do([&]() { widget = CreateAndShowTestDialog(); }),
      // Wait for the dialog to be visible and focused.
      WaitForShow(kDialogViewId),
      WaitForState(views::test::kCurrentWidgetFocus,
                   [&]() { return widget.get(); }),
      Check([&]() { return widget->IsActive(); }, "Verify dialog is active"),

      // Activate the browser window.
      Do([&]() { browser()->window()->Activate(); }),

      // Check that the browser window is active.
      WaitForState(views::test::kCurrentWidgetFocus,
                   [&]() {
                     return BrowserView::GetBrowserViewForBrowser(browser())
                         ->GetWidget();
                   }),

      // Focus the contents views::WebView. The focus should be moved to the
      // dialog.
      WithView(ContentsWebView::kContentsWebViewElementId,
               [](views::View* contents_web_view) {
                 contents_web_view->RequestFocus();
               }),
      WaitForState(views::test::kCurrentWidgetFocus,
                   [&]() { return widget.get(); }));
}
#endif  // !BUILDFLAG(IS_MAC)

// Tests that the browser window can be activated when a modal dialog is active.
// Note that because the WebView forwards the focus to the dialog, the browser
// window risks not being activated in the following scenario:
// 1. The WebView in the browser window is focused.
// 2. A modal dialog is shown.
// 3. The browser window is activated, e.g. by clicking on the omnibox.
// 4. The WebView is focused due to the focus restoration.
// 5. The focus is forwarded to the dialog.
// The end result is that the browser window fails to be activated when the
// omnibox is clicked after step 3. This test ensures that this does not happen.
// Implementation-wise, this is achieved by not forwarding the focus to the
// dialog during the focus restoration.
IN_PROC_BROWSER_TEST_F(TabDialogManagerDesktopWidgetUiTest,
                       ActivateBrowserWindowWhenModalIsActive) {
  std::unique_ptr<views::Widget> widget;
  views::Widget* browser_widget = nullptr;

  RunTestSequence(
      Do([&]() {
        browser_widget =
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
      }),
      ObserveState(views::test::kCurrentWidgetFocus),
      ObserveState(
          views::test::kCurrentFocusedViewId,
          BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()),
      // Click on the tab container and check that it has focus.
      MoveMouseTo(ContentsWebView::kContentsWebViewElementId), ClickMouse(),
      WaitForState(views::test::kCurrentFocusedViewId,
                   ContentsWebView::kContentsWebViewElementId),
      // Show a dialog.
      Do([&]() { widget = CreateAndShowTestDialog(); }),
      // Wait for the dialog to be visible and focused.
      WaitForShow(kDialogViewId),
      WaitForState(views::test::kCurrentWidgetFocus,
                   [&]() { return widget.get(); }),
      Check([&]() { return widget->IsActive(); }, "Verify dialog is active"),
      WithView(ContentsWebView::kContentsWebViewElementId,
               [&](views::View* contents_web_view) {
                 EXPECT_EQ(
                     browser_widget->GetFocusManager()->GetStoredFocusView(),
                     contents_web_view);
               }),

      // Activate the browser window.
      Do([&]() { browser()->window()->Activate(); }),

      // Check that the browser window is active and the dialog is not.
      WaitForState(views::test::kCurrentWidgetFocus, std::ref(browser_widget)));

  EXPECT_TRUE(browser()->window()->IsActive());
  EXPECT_FALSE(widget->IsActive());
  // Ensures that the tab modal is not asynchronously activated.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(browser()->window()->IsActive());
  EXPECT_FALSE(widget->IsActive());
}

}  // namespace tabs
