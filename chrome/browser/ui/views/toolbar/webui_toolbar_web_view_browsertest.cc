// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/command_line.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

class WebUIToolbarWebViewPixelBrowserTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewPixelBrowserTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Force the color mode to light to avoid flakiness.
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest, Basic) {
  ui::TrackedElement* element = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    element =
        BrowserElements::From(browser())->GetElement(kReloadButtonElementId);
    return element != nullptr;
  }));
  ASSERT_TRUE(element);
  views::TrackedElementViews* webui_toolbar_view_element =
      element->AsA<views::TrackedElementViews>();

  ASSERT_TRUE(webui_toolbar_view_element);
  WebUIToolbarWebView* webui_toolbar_view =
      views::AsViewClass<WebUIToolbarWebView>(
          webui_toolbar_view_element->view());
  ASSERT_TRUE(webui_toolbar_view);
  ASSERT_EQ(webui_toolbar_view->children().size(), 1u);
  views::WebView* web_view = views::AsViewClass<views::WebView>(
      webui_toolbar_view->children()[0].get());
  ASSERT_TRUE(web_view);

  // Wait for the WebView to finish composition.
  content::WaitForCopyableViewInWebContents(web_view->GetWebContents());
  // Assert that WebContents is not loading, as it affects the state of the
  // reload button.
  ASSERT_FALSE(web_view->GetWebContents()->IsLoading());
  // The WebView should be using the light color mode.
  ASSERT_EQ(web_view->GetWidget()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kLight);

  // Pixel test
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVerifyPixels)) {
    views::ViewSkiaGoldPixelDiff pixel_diff(
        "WebUIToolbarWebViewPixelBrowserTest");
    EXPECT_TRUE(pixel_diff.CompareViewScreenshot("Basic", webui_toolbar_view));
  }
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest, Accessibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ui::TrackedElement* element = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    element =
        BrowserElements::From(browser())->GetElement(kReloadButtonElementId);
    return element != nullptr;
  }));
  ASSERT_TRUE(element);
  views::TrackedElementViews* webui_toolbar_view_element =
      element->AsA<views::TrackedElementViews>();
  ASSERT_TRUE(webui_toolbar_view_element);
  WebUIToolbarWebView* webui_toolbar_view =
      views::AsViewClass<WebUIToolbarWebView>(
          webui_toolbar_view_element->view());
  ASSERT_TRUE(webui_toolbar_view);
  ASSERT_EQ(webui_toolbar_view->children().size(), 1u);
  views::WebView* web_view = views::AsViewClass<views::WebView>(
      webui_toolbar_view->children()[0].get());
  ASSERT_TRUE(web_view);

  content::WaitForCopyableViewInWebContents(web_view->GetWebContents());

  // Find accessibility node for reload button.
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Reload");
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.name = "Reload";
  ui::AXPlatformNodeDelegate* reload_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(reload_node);

  // Verify appropriate accessibility properties for reload button.
  const ui::AXNodeData& reload = reload_node->GetData();
  EXPECT_EQ(ax::mojom::Role::kButton, reload.role);
  EXPECT_EQ(true, reload.IsClickable());
  EXPECT_EQ("Reload",
            reload.GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Reload this page", reload.GetStringAttribute(
                                    ax::mojom::StringAttribute::kDescription));
  EXPECT_EQ(0, reload.GetIntAttribute(ax::mojom::IntAttribute::kHasPopup));

  // Verify enabling menu is reflected in HasPopup attribute.
  webui_toolbar_view->GetReloadControl()->SetMenuEnabled(true);
  content::WaitForAccessibilityTreeToChange(web_view->GetWebContents());
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Reload");
  reload_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(reload_node);
  EXPECT_EQ(2, reload_node->GetData().GetIntAttribute(
                   ax::mojom::IntAttribute::kHasPopup));
}
