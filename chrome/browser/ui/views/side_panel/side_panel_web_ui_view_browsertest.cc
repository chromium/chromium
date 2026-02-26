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
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/window_controller.h"
#include "extensions/browser/extension_web_contents_observer.h"
#endif

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

void QueryTabsForCurrentWindowAndCheckResults(
    content::WebContents* contents,
    Browser* browser,
    const std::string& first_tab_expected_url,
    bool first_tab_should_be_active,
    const std::string& second_tab_expected_url,
    bool second_tab_should_be_active) {
  base::ListValue list(
      content::EvalJs(contents, "chrome.tabs.query({currentWindow: true})")
          .ExtractList()
          .Clone());
  EXPECT_EQ(list.size(), browser->tab_strip_model()->count());
  EXPECT_TRUE(list[0].is_dict());
  EXPECT_TRUE(list[1].is_dict());
  {
    const base::DictValue& dict = list[0].GetDict();
    EXPECT_TRUE(dict.FindString("url"));
    EXPECT_EQ(*dict.FindString("url"), first_tab_expected_url);
    EXPECT_EQ(dict.FindBool("active"), first_tab_should_be_active);
  }
  {
    const base::DictValue& dict = list[1].GetDict();
    EXPECT_TRUE(dict.FindString("url"));
    EXPECT_EQ(*dict.FindString("url"), second_tab_expected_url);
    EXPECT_EQ(dict.FindBool("active"), second_tab_should_be_active);
  }
}

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
    SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
    side_panel_ui->SetNoDelaysForTesting(true);
    side_panel_ui->DisableAnimationsForTesting();
  }

  // Registers a per-browser-window side panel entry with the test's default
  // browser.
  void RegisterBrowserSidePanelEntry() {
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(kTestGlobalEntryId),
        base::BindRepeating(
            [](Profile* profile,
               SidePanelEntryScope& scope) -> std::unique_ptr<views::View> {
              return std::make_unique<TestSidePanelWebUIView>(
                  scope, std::make_unique<TestWebUIContentsWrapper>(profile));
            },
            browser()->profile()),
        /*default_content_width_callback=*/base::NullCallback());

    SidePanelRegistry::From(browser())->Register(std::move(entry));
  }

  // Registers a per-tab side panel entry with the test's default browser active
  // tab.
  void RegisterTabSidePanelEntry() {
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Key(kTestTabEntryId),
        base::BindRepeating(
            [](Profile* profile,
               SidePanelEntryScope& scope) -> std::unique_ptr<views::View> {
              return std::make_unique<TestSidePanelWebUIView>(
                  scope, std::make_unique<TestWebUIContentsWrapper>(profile));
            },
            browser()->profile()),
        /*default_content_width_callback=*/base::NullCallback());
    browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetTabFeatures()
        ->side_panel_registry()
        ->Register(std::move(entry));
  }
};

IN_PROC_BROWSER_TEST_F(SidePanelWebUIViewTest,
                       BrowserInterfaceSetForWindowSidePanels) {
  // Register and show a window scoped side panel.
  RegisterBrowserSidePanelEntry();
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(kTestGlobalEntryId);
  EXPECT_TRUE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(kTestGlobalEntryId)));
  content::WebContents* side_panel_webui_contents =
      side_panel_ui->GetWebContentsForTest(kTestGlobalEntryId);
  EXPECT_TRUE(side_panel_webui_contents);

  // The browser window interface should be correctly set on the webview's
  // hosted WebContents.
  EXPECT_EQ(browser(),
            webui::GetBrowserWindowInterface(side_panel_webui_contents));
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
IN_PROC_BROWSER_TEST_F(SidePanelWebUIViewTest,
                       SidePanelVerifyWindowController) {
  // Register and show a window scoped side panel.
  RegisterBrowserSidePanelEntry();
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(kTestGlobalEntryId);
  EXPECT_TRUE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(kTestGlobalEntryId)));
  content::WebContents* side_panel_webui_contents =
      side_panel_ui->GetWebContentsForTest(kTestGlobalEntryId);
  EXPECT_TRUE(side_panel_webui_contents);

  // The browser window interface should be correctly set on the webview's
  // hosted WebContents.
  EXPECT_EQ(browser(),
            webui::GetBrowserWindowInterface(side_panel_webui_contents));

  // Create another browser as a test interference.
  Browser* another_browser = CreateBrowser(browser()->profile());
  EXPECT_TRUE(another_browser);
  EXPECT_NE(browser(), another_browser);

  // Verify whether the `dispatcher` related information points to the current
  // browser's context.
  extensions::ExtensionFunctionDispatcher* const dispatcher =
      extensions::ExtensionWebContentsObserver::GetForWebContents(
          side_panel_webui_contents)
          ->dispatcher();
  EXPECT_TRUE(dispatcher);
  extensions::WindowController* window_controller =
      dispatcher->GetExtensionWindowController();
  EXPECT_TRUE(window_controller);
  EXPECT_EQ(browser(), window_controller->GetBrowserWindowInterface());
}

IN_PROC_BROWSER_TEST_F(SidePanelWebUIViewTest,
                       SidePanelQueryTabsForCurrentWindow) {
  // Register and show a window scoped side panel.
  RegisterBrowserSidePanelEntry();
  constexpr char kTestUrl1ForThisBrowser[] = "chrome://bookmarks/";
  constexpr char kTestUrl2ForThisBrowser[] = "chrome://settings/";
  constexpr char kTestUrl1ForNewBrowser[] = "chrome://history/";
  constexpr char kTestUrl2ForNewBrowser[] = "chrome://downloads/";
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(kTestGlobalEntryId);
  EXPECT_TRUE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(kTestGlobalEntryId)));

  content::WebContents* side_panel_webui_contents =
      side_panel_ui->GetWebContentsForTest(kTestGlobalEntryId);
  EXPECT_TRUE(side_panel_webui_contents);

  // The browser window interface should be correctly set on the webview's
  // hosted WebContents.
  EXPECT_EQ(browser(),
            webui::GetBrowserWindowInterface(side_panel_webui_contents));

  // Wait for load stop, because we need use `chrome.tabs` API later.
  content::WaitForLoadStop(side_panel_webui_contents);

  // Create tabs, and new window for testing.
  browser()->OpenGURL(GURL(kTestUrl1ForThisBrowser),
                      WindowOpenDisposition::CURRENT_TAB);
  browser()->OpenGURL(GURL(kTestUrl2ForThisBrowser),
                      WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetTabAtIndex(0)->GetContents());
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetTabAtIndex(1)->GetContents());

  // Test for current browser's tab.
  QueryTabsForCurrentWindowAndCheckResults(side_panel_webui_contents, browser(),
                                           kTestUrl1ForThisBrowser, true,
                                           kTestUrl2ForThisBrowser, false);

  // Activate the second tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);

  // The second tab should be activated.
  QueryTabsForCurrentWindowAndCheckResults(side_panel_webui_contents, browser(),
                                           kTestUrl1ForThisBrowser, false,
                                           kTestUrl2ForThisBrowser, true);

  // A new browser instance is created as a confounding variable, and it should
  // not interfere with API calls in the `side_panel_webui_contents`.
  Browser* new_browser = CreateBrowser(browser()->profile());
  EXPECT_TRUE(new_browser);
  new_browser->OpenGURL(GURL(kTestUrl1ForNewBrowser),
                        WindowOpenDisposition::CURRENT_TAB);
  new_browser->OpenGURL(GURL(kTestUrl2ForNewBrowser),
                        WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_EQ(new_browser->tab_strip_model()->count(), 2);
  EXPECT_EQ(new_browser->tab_strip_model()->active_index(), 0);
  content::WaitForLoadStop(
      new_browser->tab_strip_model()->GetTabAtIndex(0)->GetContents());
  content::WaitForLoadStop(
      new_browser->tab_strip_model()->GetTabAtIndex(1)->GetContents());

  // No matter how we activate the tab in the new browser, the result should
  // still remain unchanged.
  QueryTabsForCurrentWindowAndCheckResults(side_panel_webui_contents, browser(),
                                           kTestUrl1ForThisBrowser, false,
                                           kTestUrl2ForThisBrowser, true);

  new_browser->tab_strip_model()->ActivateTabAt(1);
  QueryTabsForCurrentWindowAndCheckResults(side_panel_webui_contents, browser(),
                                           kTestUrl1ForThisBrowser, false,
                                           kTestUrl2ForThisBrowser, true);
}
#endif

IN_PROC_BROWSER_TEST_F(SidePanelWebUIViewTest,
                       TabScopedSidePanel_WebUIContextSetCorrectlyOnShow) {
  // Register and show a tab scoped side panel.
  RegisterTabSidePanelEntry();
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(kTestTabEntryId);
  EXPECT_TRUE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(kTestTabEntryId)));
  content::WebContents* side_panel_webui_contents =
      side_panel_ui->GetWebContentsForTest(kTestTabEntryId);
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
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(kTestTabEntryId);
  EXPECT_TRUE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(kTestTabEntryId)));
  content::WebContents* side_panel_webui_contents =
      side_panel_ui->GetWebContentsForTest(kTestTabEntryId);
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
