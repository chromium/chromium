// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"

#include <memory>

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr SidePanelEntry::Id kTestGlobalEntryId =
    SidePanelEntry::Id::kReadingList;
constexpr SidePanelEntry::Id kTestTabEntryId =
    SidePanelEntry::Id::kAboutThisSite;

class TestWebUIContentsWrapper : public WebUIContentsWrapper {
 public:
  explicit TestWebUIContentsWrapper(Profile* profile)
      : WebUIContentsWrapper(/*webui_url=*/GURL("chrome://test"),
                             profile,
                             /*task_manager_string_id=*/1,
                             /*webui_resizes_host=*/false,
                             /*esc_closes_ui=*/false,
                             /*supports_draggable_regions=*/false,
                             /*webui_name=*/"Test") {}

  // WebUIContentsWrapper:
  void ReloadWebContents() override {}
  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<WebUIContentsWrapper> weak_ptr_factory_{this};
};

class TestSidePanelWebUIView : public SidePanelWebUIView {
 public:
  TestSidePanelWebUIView(
      SidePanelEntryScope& scope,
      std::unique_ptr<TestWebUIContentsWrapper> contents_wrapper)
      : SidePanelWebUIView(scope,
                           /*on_show_cb=*/base::RepeatingClosure(),
                           /*close_cb=*/base::RepeatingClosure(),
                           contents_wrapper.get()),
        contents_wrapper_(std::move(contents_wrapper)) {}

 private:
  std::unique_ptr<TestWebUIContentsWrapper> contents_wrapper_;
};

}  // namespace

class SidePanelWebUIViewTest : public InProcessBrowserTest {
 public:
  SidePanelWebUIViewTest() = default;
  SidePanelWebUIViewTest(const SidePanelWebUIViewTest&) = delete;
  SidePanelWebUIViewTest& operator=(const SidePanelWebUIViewTest&) = delete;
  ~SidePanelWebUIViewTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    GetSidePanelCoordinator()->SetNoDelaysForTesting(true);
    GetSidePanelCoordinator()->DisableAnimationsForTesting();
  }

  // Registers a per-browser-window side panel entry with the test's default
  // browser.
  void RegisterBrowserSidePanelEntry() {
    auto entry = std::make_unique<SidePanelEntry>(
        kTestGlobalEntryId,
        base::BindRepeating(
            [](Profile* profile,
               SidePanelEntryScope& scope) -> std::unique_ptr<views::View> {
              return std::make_unique<TestSidePanelWebUIView>(
                  scope, std::make_unique<TestWebUIContentsWrapper>(profile));
            },
            browser()->profile()));

    GetSidePanelCoordinator()->GetWindowRegistry()->Register(std::move(entry));
  }

  // Registers a per-tab side panel entry with the test's default browser active
  // tab.
  void RegisterTabSidePanelEntry() {
    auto entry = std::make_unique<SidePanelEntry>(
        kTestTabEntryId,
        base::BindRepeating(
            [](Profile* profile,
               SidePanelEntryScope& scope) -> std::unique_ptr<views::View> {
              return std::make_unique<TestSidePanelWebUIView>(
                  scope, std::make_unique<TestWebUIContentsWrapper>(profile));
            },
            browser()->profile()));
    browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetTabFeatures()
        ->side_panel_registry()
        ->Register(std::move(entry));
  }

  SidePanelCoordinator* GetSidePanelCoordinator() {
    return browser()->GetFeatures().side_panel_coordinator();
  }
};

IN_PROC_BROWSER_TEST_F(SidePanelWebUIViewTest,
                       BrowserInterfaceSetForWindowSidePanels) {
  // Register and show a window scoped side panel.
  RegisterBrowserSidePanelEntry();
  GetSidePanelCoordinator()->Show(kTestGlobalEntryId);
  EXPECT_EQ(GetSidePanelCoordinator()->GetCurrentEntryId(), kTestGlobalEntryId);
  content::WebContents* side_panel_webui_contents =
      GetSidePanelCoordinator()->GetWebContentsForTest(kTestGlobalEntryId);
  EXPECT_TRUE(side_panel_webui_contents);

  // The browser window interface should be correctly set on the webview's
  // hosted WebContents.
  EXPECT_EQ(browser(),
            webui::GetBrowserWindowInterface(side_panel_webui_contents));
}

IN_PROC_BROWSER_TEST_F(SidePanelWebUIViewTest,
                       TabScopedSidePanel_WebUIContextSetCorrectlyOnShow) {
  // Register and show a tab scoped side panel.
  RegisterTabSidePanelEntry();
  GetSidePanelCoordinator()->Show(kTestTabEntryId);
  EXPECT_EQ(GetSidePanelCoordinator()->GetCurrentEntryId(), kTestTabEntryId);
  content::WebContents* side_panel_webui_contents =
      GetSidePanelCoordinator()->GetWebContentsForTest(kTestTabEntryId);
  EXPECT_TRUE(side_panel_webui_contents);

  // The browser and window interface should be correctly set on the webview's
  // hosted WebContents.
  tabs::TabInterface* tab_interface =
      browser()->tab_strip_model()->GetActiveTab();
  EXPECT_EQ(browser(),
            webui::GetBrowserWindowInterface(side_panel_webui_contents));
  EXPECT_EQ(tab_interface, webui::GetTabInterface(side_panel_webui_contents));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelWebUIViewTest,
    TabScopedSidePanel_WebUIContextSetCorrectlyAfterTabDiscard) {
  // Create a browser with 2 tabs.
  content::WebContents* tab_contents =
      chrome::AddAndReturnTabAt(browser(), GURL(url::kAboutBlankURL), 1, true);
  tabs::TabInterface* tab_interface =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  EXPECT_EQ(tab_interface, browser()->tab_strip_model()->GetActiveTab());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Register and show a tab scoped side panel.
  RegisterTabSidePanelEntry();
  GetSidePanelCoordinator()->Show(kTestTabEntryId);
  EXPECT_EQ(GetSidePanelCoordinator()->GetCurrentEntryId(), kTestTabEntryId);
  content::WebContents* side_panel_webui_contents =
      GetSidePanelCoordinator()->GetWebContentsForTest(kTestTabEntryId);
  EXPECT_TRUE(side_panel_webui_contents);

  // The browser and window interface should be correctly set on the webview's
  // hosted WebContents.
  EXPECT_EQ(browser(),
            webui::GetBrowserWindowInterface(side_panel_webui_contents));
  EXPECT_EQ(tab_interface, webui::GetTabInterface(side_panel_webui_contents));

  // Discard the tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_NE(browser()->tab_strip_model()->GetActiveTab(), tab_interface);
  auto* lifecycle_unit =
      resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
          tab_contents);
  lifecycle_unit->DiscardTab(mojom::LifecycleUnitDiscardReason::URGENT);
  EXPECT_EQ(mojom::LifecycleUnitState::DISCARDED,
            lifecycle_unit->GetTabState());
  tab_contents = browser()->tab_strip_model()->GetTabAtIndex(1)->GetContents();

  // The tab and browser interfaces should remain associated with the tab
  // contents after discard.
  EXPECT_EQ(browser(), webui::GetBrowserWindowInterface(tab_contents));
  EXPECT_EQ(tab_interface, webui::GetTabInterface(tab_contents));
}
