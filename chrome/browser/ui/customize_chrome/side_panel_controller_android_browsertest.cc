// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/customize_chrome/side_panel_controller_android.h"

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/thin_webview/tab_thin_web_view_host.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/test/android/side_panel_android_browser_test_base.h"
#include "components/search/ntp_features.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

namespace {

using ::thin_webview::android::TabThinWebViewHost;

constexpr char kTestUrl[] = "about:blank";

SidePanelEntryKey CustomizeChromeKey() {
  return SidePanelEntryKey(SidePanelEntryId::kCustomizeChrome);
}

customize_chrome::SidePanelControllerAndroid* GetCustomizeChromeController(
    tabs::TabInterface* tab) {
  return static_cast<customize_chrome::SidePanelControllerAndroid*>(
      customize_chrome::SidePanelController::Get(
          tab->GetUnownedUserDataHost()));
}

// Returns the `ui::WindowAndroid` backing `browser`.
ui::WindowAndroid* GetWindowAndroid(BrowserWindowInterface* browser) {
  return browser->GetWindow()->GetNativeWindow();
}

// Makes the side panel open and swap content without animations or delays, so
// that tests only need to wait on the side panel's own state.
void DisableSidePanelTimingEffects(SidePanelUI* side_panel_ui) {
  side_panel_ui->DisableAnimationsForTesting();
  side_panel_ui->SetNoDelaysForTesting(true);
}

// Waits until the side panel reports that Customize Chrome is being shown.
void WaitUntilCustomizeChromeShowing(SidePanelUI* side_panel_ui) {
  ASSERT_TRUE(base::test::RunUntil([side_panel_ui]() {
    return side_panel_ui->IsSidePanelEntryShowing(CustomizeChromeKey());
  })) << "Customize Chrome never became the showing side panel entry.";
}

}  // namespace

// Browser tests for the Android implementation of the "Customize Chrome" side
// panel, which hosts its WebContents in a ThinWebView.
class CustomizeChromeSidePanelAndroidBrowserTest
    : public SidePanelAndroidBrowserTestBase {
 protected:
  CustomizeChromeSidePanelAndroidBrowserTest() {
    ntp_feature_list_.InitAndEnableFeature(
        ntp_features::kNtpCustomizeWebUiAndroid);
  }

  void SetUpOnMainThread() override {
    SidePanelAndroidBrowserTestBase::SetUpOnMainThread();
    browser_ = GetLastActiveBrowser();
    tab_list_ = TabListInterface::From(browser_);
    side_panel_ui_ = SidePanelUI::From(browser_);
    DisableSidePanelTimingEffects(side_panel_ui_);
  }

  void TearDownOnMainThread() override {
    std::vector<BrowserWindowInterface*> windows =
        GetAllBrowserWindowInterfaces();
    for (BrowserWindowInterface* window : windows) {
      if (window == browser_) {
        continue;
      }
      if (ui::BaseWindow* base_window = window->GetWindow()) {
        base_window->Close();
      }
    }
    SidePanelAndroidBrowserTestBase::TearDownOnMainThread();
  }

  // Opens a tab and waits until its navigation has registered the Customize
  // Chrome side panel entry.
  tabs::TabInterface* OpenTabWithCustomizeChromeEntry() {
    tabs::TabInterface* tab =
        tab_list_->OpenTab(GURL(kTestUrl), tab_list_->GetTabCount());
    EXPECT_TRUE(base::test::RunUntil([tab]() {
      SidePanelRegistry* registry = SidePanelRegistry::From(tab);
      return registry && registry->GetEntryForKey(CustomizeChromeKey());
    }));
    return tab;
  }

  // Activates `tab` and shows its Customize Chrome side panel entry.
  void ShowCustomizeChrome(tabs::TabInterface* tab) {
    tab_list_->ActivateTab(tab->GetHandle());
    side_panel_ui_->Show(CustomizeChromeKey(),
                         SidePanelOpenTrigger::kToolbarButton,
                         /*suppress_animations=*/true);
    WaitUntilCustomizeChromeShowing(side_panel_ui_);
  }

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<TabListInterface> tab_list_ = nullptr;
  raw_ptr<SidePanelUI> side_panel_ui_ = nullptr;

 private:
  base::test::ScopedFeatureList ntp_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    CustomizeChromeSidePanelAndroidBrowserTest,
    Show_CreatesThinWebViewHostAndKeepsItAsTheWebContentsDelegate) {
  tabs::TabInterface* tab = OpenTabWithCustomizeChromeEntry();
  auto* controller = GetCustomizeChromeController(tab);
  ASSERT_NE(nullptr, controller);

  ShowCustomizeChrome(tab);

  TabThinWebViewHost* host = controller->GetWebContentsHostForTesting();
  ASSERT_NE(nullptr, host);
  EXPECT_TRUE(host->HasViewForTesting());

  // The WebContents is attached to the ThinWebView's own WindowAndroid, which
  // in turn is scoped to the Activity hosting the tab.
  content::WebContents* side_panel_contents =
      controller->GetWebContentsForTesting();
  ASSERT_NE(nullptr, side_panel_contents);
  EXPECT_NE(nullptr, side_panel_contents->GetTopLevelNativeWindow());

  // Attaching to the ThinWebView must not replace the host as the delegate,
  // otherwise link clicks in the side panel would bypass OpenURLFromTab().
  EXPECT_EQ(static_cast<content::WebContentsDelegate*>(host),
            side_panel_contents->GetDelegate());
}

// The ThinWebView and its WindowAndroid are scoped to the Activity hosting the
// tab, so they must be destroyed before the tab leaves that Activity, and
// recreated in the Activity the tab moves to. The WebContents is tab-scoped, so
// it must survive the move.
IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelAndroidBrowserTest,
                       ReparentTab_RecreatesThinWebViewHostInTheNewWindow) {
  // Arrange: A source window with 2 tabs, so that it stays alive after the
  // reparenting, showing Customize Chrome in the second tab.
  OpenTabWithCustomizeChromeEntry();
  tabs::TabInterface* tab = OpenTabWithCustomizeChromeEntry();
  auto* controller = GetCustomizeChromeController(tab);
  ASSERT_NE(nullptr, controller);
  TabThinWebViewHost* host = controller->GetWebContentsHostForTesting();
  ASSERT_NE(nullptr, host);

  ShowCustomizeChrome(tab);
  ASSERT_TRUE(host->HasViewForTesting());
  content::WebContents* side_panel_contents =
      controller->GetWebContentsForTesting();
  ASSERT_NE(nullptr, side_panel_contents);
  ui::WindowAndroid* src_side_panel_window =
      side_panel_contents->GetTopLevelNativeWindow();
  ASSERT_NE(nullptr, src_side_panel_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(browser_->GetProfile());
  SidePanelUI* dst_side_panel_ui = SidePanelUI::From(dst_window);
  DisableSidePanelTimingEffects(dst_side_panel_ui);

  // Arrange: Observe the point at which the tab leaves the source Activity.
  // Callbacks run in registration order and `TabThinWebViewHost` registers at
  // tab creation time, so this runs after the host has reacted.
  std::optional<bool> had_view_when_detached;
  std::optional<bool> had_window_when_detached;
  base::CallbackListSubscription subscription =
      tab->RegisterWillDetach(base::BindLambdaForTesting(
          [&](tabs::TabInterface*, tabs::TabInterface::DetachReason) {
            had_view_when_detached = host->HasViewForTesting();
            had_window_when_detached =
                side_panel_contents->GetTopLevelNativeWindow() != nullptr;
          }));

  // Act:
  tab_list_->MoveTabToWindow(tab->GetHandle(), dst_window->GetSessionID(),
                             /*destination_index=*/0);
  WaitUntilCustomizeChromeShowing(dst_side_panel_ui);

  // Assert: The Activity-scoped View, and the WebContents' reference to the
  // Activity-scoped window, were both dropped before the tab left the source
  // Activity...
  ASSERT_TRUE(had_view_when_detached.has_value());
  ASSERT_TRUE(had_window_when_detached.has_value());
  EXPECT_FALSE(had_view_when_detached.value());
  EXPECT_FALSE(had_window_when_detached.value());

  // ...and the View was recreated in the destination window, hosting the same,
  // tab-scoped WebContents in a new, destination-Activity-scoped window.
  EXPECT_TRUE(host->HasViewForTesting());
  EXPECT_EQ(side_panel_contents, controller->GetWebContentsForTesting());
  EXPECT_EQ(GetWindowAndroid(dst_window),
            tab->GetContents()->GetTopLevelNativeWindow());
  ui::WindowAndroid* dst_side_panel_window =
      side_panel_contents->GetTopLevelNativeWindow();
  EXPECT_NE(nullptr, dst_side_panel_window);
  EXPECT_NE(src_side_panel_window, dst_side_panel_window);
}
