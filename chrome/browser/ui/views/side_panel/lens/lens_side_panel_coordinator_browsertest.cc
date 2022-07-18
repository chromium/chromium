// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/browser/ui/lens/lens_side_panel_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/search_engines/template_url_service.h"
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
#include "ui/views/controls/button/image_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace {

constexpr char kCloseAction[] = "LensUnifiedSidePanel.HideSidePanel";

// Maintains image search test state. In particular, note that |menu_observer_|
// must live until the right-click completes asynchronously.
class SearchImageWithUnifiedSidePanel : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeaturesAndParameters(
        {{lens::features::kLensStandalone,
          {{lens::features::kEnableSidePanelForLens.name, "true"},
           {lens::features::kEnableLensSidePanelFooter.name, "true"}}},
         {features::kUnifiedSidePanel, {{}}}},
        {});
    InProcessBrowserTest::SetUp();
  }

  void SetupUnifiedSidePanel() {
    // ensures that the lens side panel coordinator is open and is valid when
    // running the search
    lens::CreateLensUnifiedSidePanelEntryForTesting(browser());
    SetupAndLoadValidImagePage();
    // The browser should open a side panel with the image.
    AttemptLensImageSearch();

    // We need to verify the contents before opening the side panel
    content::WebContents* contents =
        lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());
    // // Wait for the side panel to open and finish loading web contents.
    content::TestNavigationObserver nav_observer(contents);
    nav_observer.Wait();
  }

  void SetupAndLoadValidImagePage() {
    constexpr char kValidImage[] = "/image_search/valid.png";
    SetupAndLoadImagePage(kValidImage);
  }

  void SetupAndLoadImagePage(const std::string& image_path) {
    // The test server must start first, so that we know the port that the test
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());
    SetupImageSearchEngine();

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
    LOG(INFO) << "setURL: "
              << embedded_test_server()->GetURL(kSearchURL).spec();
    data.SetURL(embedded_test_server()->GetURL(kSearchURL).spec());
    LOG(INFO) << "image_url: " << GetImageSearchURL().spec();
    data.image_url = GetImageSearchURL().spec();
    LOG(INFO) << "image search post params: " << kImageSearchPostParams;
    data.image_url_post_params = kImageSearchPostParams;

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void TearDownInProcessBrowserTestFixture() override {
    menu_observer_.reset();
  }

  SidePanelCoordinator* GetSidePanelCoordinator() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->side_panel_coordinator();
  }

  LensSidePanelCoordinator* GetLensSidePanelCoordinator() {
    return LensSidePanelCoordinator::GetOrCreateForBrowser(browser());
  }

  SidePanel* GetRightAlignedSidePanel() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->right_aligned_side_panel();
  }

  std::unique_ptr<ContextMenuNotificationObserver> menu_observer_;
  base::UserActionTester user_action_tester;
};

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       ImageSearchWithValidImageOpensUnifiedSidePanel) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetRightAlignedSidePanel()->GetVisible());

  content::WebContents* contents =
      lens::GetLensUnifiedSidePanelWebContentsForTesting(browser());

  std::string expected_content = GetLensImageSearchURL().GetContent();
  std::string side_panel_content = contents->GetLastCommittedURL().GetContent();
  // Match strings up to the query.
  std::size_t query_start_pos = side_panel_content.find("?");
  EXPECT_EQ(expected_content.substr(0, query_start_pos),
            side_panel_content.substr(0, query_start_pos));
  // Match the query parameters, without the value of start_time.
  EXPECT_THAT(side_panel_content,
              testing::MatchesRegex(".*ep=ccm&s=csp&st=\\d+&p=somepayload"));
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       ClosingSidePanelDeregistersLensViewAndLogsCloseMetric) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetRightAlignedSidePanel()->GetVisible());

  GetSidePanelCoordinator()->Close();

  EXPECT_FALSE(GetRightAlignedSidePanel()->GetVisible());
  auto* last_active_entry =
      GetSidePanelCoordinator()->GetCurrentSidePanelEntryForTesting();
  EXPECT_EQ(last_active_entry, nullptr);
  EXPECT_EQ(
      GetSidePanelCoordinator()->GetGlobalSidePanelRegistry()->GetEntryForId(
          SidePanelEntry::Id::kLens),
      nullptr);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kCloseAction));
}

IN_PROC_BROWSER_TEST_F(SearchImageWithUnifiedSidePanel,
                       OpenInNewTabOpensInNewTabAndClosesSidePanel) {
  SetupUnifiedSidePanel();
  EXPECT_TRUE(GetRightAlignedSidePanel()->GetVisible());

  auto did_open_results =
      GetLensSidePanelCoordinator()->OpenResultsInNewTabForTesting();

  EXPECT_TRUE(did_open_results);
  EXPECT_FALSE(GetRightAlignedSidePanel()->GetVisible());
}

}  // namespace
