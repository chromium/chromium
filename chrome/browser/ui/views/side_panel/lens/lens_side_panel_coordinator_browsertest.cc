// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_testing_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace {

constexpr char kCloseAction[] = "LensUnifiedSidePanel.HideSidePanel";
constexpr char kExpectedLensSidePanelContentUrlRegex[] =
    ".*ep=ccm&re=dcsp&s=4&st=\\d+&lm=.+&p=somepayload&ep=ccmupload&"
    "sideimagesearch=1&vpw=\\d+&vph=\\d+";
constexpr char kExpected3PDseSidePanelContentUrlRegex[] =
    ".*p=somepayload&sideimagesearch=1&vpw=\\d+&vph=\\d+";
constexpr char kExpectedNewTabContentUrlRegex[] = ".*p=somepayload";

// Maintains image search test state. In particular, note that |menu_observer_|
// must live until the right-click completes asynchronously.
class SearchImageWithUnifiedSidePanel : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    // The test server must start first, so that we know the port that the test
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());

    // Lens side panel throttle requests outside of the initial subdomain so
    // need to set the HomepageURLForLens to be the same host as our embedded
    // test server.
    base::test::ScopedFeatureList features;
    features.InitWithFeaturesAndParameters(
        {{lens::features::kLensStandalone,
          {{lens::features::kHomepageURLForLens.name,
            GetLensImageSearchURL().spec()}}},
         {lens::features::kEnableImageSearchSidePanelFor3PDse, {{}}}},
        {});
    InProcessBrowserTest::SetUp();
  }

  void SetupUnifiedSidePanel(bool for_google = true) {
    SetupAndLoadValidImagePage();
    // Ensures that the lens side panel coordinator is open and is valid when
    // running the search.
    lens::CreateLensUnifiedSidePanelEntryForTesting(browser());
    // The browser should open a side panel with the image.

    if (for_google) {
      AttemptLensImageSearch();
    } else {
      Attempt3pDseImageSearch();
    }

    // We need to verify the contents before opening the side panel.
    content::WebContents* contents =
        lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());
    // Wait for the side panel to open and finish loading web contents.
    content::TestNavigationObserver nav_observer(contents);
    nav_observer.Wait();
  }

  void SetupAndLoadValidImagePage() {
    constexpr char kValidImage[] = "/image_search/valid.png";
    SetupAndLoadImagePage(kValidImage);
  }

  void SetupAndLoadImagePage(const std::string& image_path) {
    // Go to a page with an image in it. The test server doesn't serve the image
    // with the right MIME type, so use a data URL to make a page containing it.
    GURL image_url(embedded_test_server()->GetURL(image_path));
    GURL page("data:text/html,<img src='" + image_url.spec() + "'>");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));
  }

  void AttemptLensImageSearch() {
    // |menu_observer_| will cause the search lens for image menu item to be
    // clicked.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE);
    RightClickImage();
  }

  void Attempt3pDseImageSearch() {
    // |menu_observer_| will cause the search web for image menu item to be
    // clicked.
    menu_observer_ = std::make_unique<ContextMenuNotificationObserver>(
        IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE);
    RightClickImage();
  }

  // Right-click where the image should be.
  void RightClickImage() {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::SimulateMouseClickAt(tab, 0, blink::WebMouseEvent::Button::kRight,
                                  gfx::Point(15, 15));
  }

  GURL GetImageSearchURL() {
    constexpr char kImageSearchURL[] = "/imagesearch?p=somepayload";
    return embedded_test_server()->GetURL(kImageSearchURL);
  }

  GURL GetLensImageSearchURL() {
    constexpr char kLensImageSearchURL[] = "/imagesearch?p=somepayload&ep=ccm";
    return embedded_test_server()->GetURL(kLensImageSearchURL);
  }

  GURL GetBadLensImageSearchURL() {
    constexpr char kBadLensImageSearchURL[] = "/imagesearch";
    return embedded_test_server()->GetURL(kBadLensImageSearchURL);
  }

  void SetupImageSearchEngine() {
    constexpr char16_t kShortName[] = u"test";
    constexpr char kSearchURL[] = "/search?q={searchTerms}";
    constexpr char kImageSearchPostParams[] = "thumb={google:imageThumbnail}";

    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.SetShortName(kShortName);
    data.SetKeyword(data.short_name());
    data.SetURL(embedded_test_server()->GetURL(kSearchURL).spec());
    data.image_url = GetImageSearchURL().spec();
    data.image_url_post_params = kImageSearchPostParams;
    data.side_image_search_param = "sideimagesearch";

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void TearDownInProcessBrowserTestFixture() override {
    menu_observer_.reset();
  }

  SidePanelCoordinator* GetSidePanelCoordinator() {
    return browser()->GetFeatures().side_panel_coordinator();
  }

  LensSidePanelCoordinator* GetLensSidePanelCoordinator() {
    return LensSidePanelCoordinator::GetOrCreateForBrowser(browser());
  }

  SidePanel* GetUnifiedSidePanel() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->unified_side_panel();
  }

  // Ensures the last request seen by |web_contents| contained encoded image
  // data
  void ExpectThatRequestContainsImageData(content::WebContents* web_contents) {
    auto* last_entry = web_contents->GetController().GetLastCommittedEntry();
    EXPECT_TRUE(last_entry);
    EXPECT_TRUE(last_entry->GetHasPostData());

    std::string post_data = last_entry->GetPageState().ToEncodedData();
    std::string image_bytes = lens::GetImageBytesFromEncodedPostData(post_data);
    EXPECT_FALSE(image_bytes.empty());
  }

  std::unique_ptr<ContextMenuNotificationObserver> menu_observer_;
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
};

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       ImageSearchWithValidImageOpensUnifiedSidePanelForLens) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  content::WebContents* contents =
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());

  std::string expected_content = GetLensImageSearchURL().GetContent();
  std::string side_panel_content = contents->GetLastCommittedURL().GetContent();
  // Match strings up to the query.
  std::size_t query_start_pos = side_panel_content.find("?");
  EXPECT_EQ(expected_content.substr(0, query_start_pos),
            side_panel_content.substr(0, query_start_pos));
  EXPECT_TRUE(GetLensSidePanelCoordinator()->IsLaunchButtonEnabledForTesting());
  // Match the query parameters, without the value of start_time.
  EXPECT_THAT(side_panel_content,
              testing::MatchesRegex(kExpectedLensSidePanelContentUrlRegex));
  ExpectThatRequestContainsImageData(contents);

  // Ensure SidePanel.OpenTrigger was recorded correctly.
  histogram_tester.ExpectBucketCount("SidePanel.OpenTrigger",
                                     SidePanelOpenTrigger::kLensContextMenu, 1);
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       ImageSearchWithValidImageOpensUnifiedSidePanelFor3PDse) {
  SetupImageSearchEngine();
  SetupUnifiedSidePanel(/**for_google*/ false);
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  content::WebContents* contents =
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());

  std::string expected_content = GetImageSearchURL().GetContent();
  std::string side_panel_content = contents->GetLastCommittedURL().GetContent();
  // Match strings up to the query.
  std::size_t query_start_pos = side_panel_content.find("?");
  EXPECT_EQ(expected_content.substr(0, query_start_pos),
            side_panel_content.substr(0, query_start_pos));
  EXPECT_TRUE(GetLensSidePanelCoordinator()->IsLaunchButtonEnabledForTesting());
  EXPECT_THAT(side_panel_content,
              testing::MatchesRegex(kExpected3PDseSidePanelContentUrlRegex));
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       EnablesOpenInNewTabForLensErrorUrl) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  // Make URL have payload param with no value ("p=")
  auto error_url = embedded_test_server()->GetURL("/imagesearch?p=&ep=ccm");
  auto url_params = content::OpenURLParams(
      error_url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  auto load_url_params =
      content::NavigationController::LoadURLParams(url_params);
  lens::GetLensUnifiedSidePanelWebContentsForTesting(browser())
      ->GetController()
      .LoadURLWithParams(load_url_params);

  // Wait for the side panel to open and finish loading web contents.
  content::TestNavigationObserver nav_observer(
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser()));
  nav_observer.Wait();

  EXPECT_TRUE(GetLensSidePanelCoordinator()->IsLaunchButtonEnabledForTesting());
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       EnablesOpenInNewTabForLensAlternateErrorUrl) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  // Make URL have payload param with no value ("p=")
  auto error_url = embedded_test_server()->GetURL("/imagesearch?p");
  auto url_params = content::OpenURLParams(
      error_url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  auto load_url_params =
      content::NavigationController::LoadURLParams(url_params);
  lens::GetLensUnifiedSidePanelWebContentsForTesting(browser())
      ->GetController()
      .LoadURLWithParams(load_url_params);

  // Wait for the side panel to open and finish loading web contents.
  content::TestNavigationObserver nav_observer(
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser()));
  nav_observer.Wait();

  EXPECT_TRUE(GetLensSidePanelCoordinator()->IsLaunchButtonEnabledForTesting());
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       EnablesOpenInNewTabForAnyUrlForNonGoogleDse) {
  SetupImageSearchEngine();
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  // Make URL have no payload param ("p=")
  auto url =
      content::OpenURLParams(GetBadLensImageSearchURL(), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false);
  auto load_url_params = content::NavigationController::LoadURLParams(url);
  lens::GetLensUnifiedSidePanelWebContentsForTesting(browser())
      ->GetController()
      .LoadURLWithParams(load_url_params);

  // Wait for the side panel to open and finish loading web contents.
  content::TestNavigationObserver nav_observer(
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser()));
  nav_observer.Wait();

  EXPECT_TRUE(GetLensSidePanelCoordinator()->IsLaunchButtonEnabledForTesting());
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       ClosingSidePanelDeregistersLensViewAndLogsCloseMetric) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  GetSidePanelCoordinator()->Close();

  EXPECT_FALSE(GetUnifiedSidePanel()->GetVisible());
  auto* last_active_entry =
      GetSidePanelCoordinator()->GetCurrentSidePanelEntryForTesting();
  EXPECT_EQ(last_active_entry, nullptr);
  EXPECT_EQ(
      browser()
          ->browser_window_features()
          ->side_panel_coordinator()
          ->GetWindowRegistry()
          ->GetEntryForKey(SidePanelEntry::Key(SidePanelEntry::Id::kLens)),
      nullptr);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kCloseAction));
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       OpenInNewTabOpensInNewTabAndClosesSidePanel) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  auto did_open_results =
      GetLensSidePanelCoordinator()->OpenResultsInNewTabForTesting();

  EXPECT_TRUE(did_open_results);
  EXPECT_FALSE(GetUnifiedSidePanel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       UserClickToSameDomainProceedsInSidePanel) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  // Simulate a user click
  GURL nav_url = embedded_test_server()->GetURL("/new_path");
  lens::GetLensUnifiedSidePanelWebContentsForTesting(browser())
      ->GetController()
      .LoadURL(nav_url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
               std::string());

  // Wait for the side panel to finish loading web contents.
  content::TestNavigationObserver nav_observer(
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser()));
  nav_observer.Wait();

  content::WebContents* contents =
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());
  auto side_panel_url = contents->GetLastCommittedURL();

  EXPECT_EQ(side_panel_url, nav_url);
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       UserClickToSeperateDomainOpensNewTab) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  GURL nav_url = GURL("http://new.domain.com/");
  auto* side_panel_contents =
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());

  // Simulate a user click
  lens::GetLensUnifiedSidePanelWebContentsForTesting(browser())
      ->GetController()
      .LoadURL(nav_url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
               std::string());

  // Get the result URL in the new tab to verify.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);

  GURL side_panel_content = side_panel_contents->GetLastCommittedURL();
  GURL new_tab_contents = new_tab->GetLastCommittedURL();

  EXPECT_NE(side_panel_content, nav_url);
  EXPECT_EQ(GetImageSearchURL().host(), side_panel_content.host());
  EXPECT_EQ(new_tab_contents, nav_url);
}

class SearchImageWithSidePanel3PDseDisabled
    : public SearchImageWithUnifiedSidePanel {
 protected:
  void SetUp() override {
    // The test server must start first, so that we know the port that the test
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());

    base::test::ScopedFeatureList features;
    features.InitWithFeaturesAndParameters(
        {
            {lens::features::kLensStandalone,
             {{lens::features::kHomepageURLForLens.name,
               GetLensImageSearchURL().spec()}}},
        },
        {lens::features::kEnableImageSearchSidePanelFor3PDse});
    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(SearchImageWithSidePanel3PDseDisabled,
                       ImageSearchFor3PDSEWithValidImageOpensInNewTab) {
  SetupImageSearchEngine();
  SetupAndLoadValidImagePage();

  // Ensures that the lens side panel coordinator is open and is valid when
  // running the search.
  lens::CreateLensUnifiedSidePanelEntryForTesting(browser());
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  // The browser should open in a new tab with the image.
  Attempt3pDseImageSearch();

  // Get the result URL in the new tab and verify.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  std::string new_tab_content = new_tab->GetLastCommittedURL().GetContent();
  EXPECT_THAT(new_tab_content,
              testing::MatchesRegex(kExpectedNewTabContentUrlRegex));

  ExpectThatRequestContainsImageData(new_tab);

  content::WebContents* contents =
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());
  std::string side_panel_content = contents->GetLastCommittedURL().GetContent();
  EXPECT_NE(side_panel_content, new_tab_content);
}

}  // namespace
