// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace lens {

namespace {

// Returns the viewport size in physical pixels.
gfx::Size GetViewportPhysicalSize(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  return gfx::ScaleToCeiledSize(
      web_contents->GetViewBounds().size(),
      web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor());
}

}  // namespace

class TestTabContextualizationController
    : public TabContextualizationController {
 public:
  explicit TestTabContextualizationController(tabs::TabInterface* tab)
      : TabContextualizationController(tab) {}
  ~TestTabContextualizationController() override = default;

  void SetIsPageContextEligible(bool is_eligible) {
    is_page_context_eligible_ = is_eligible;
  }

 protected:
  bool IsPageContextEligible(const GURL& url,
                             const std::vector<optimization_guide::FrameMetadata>&
                                 frame_metadata) override {
    return is_page_context_eligible_;
  }

 private:
  bool is_page_context_eligible_ = true;
};

class TabContextualizationControllerBrowserTest : public InProcessBrowserTest {
 public:
  TabContextualizationControllerBrowserTest() {
    controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(
                base::BindRepeating([](tabs::TabInterface& tab) {
                  return std::make_unique<TestTabContextualizationController>(
                      &tab);
                }));
  }
  ~TabContextualizationControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  TestTabContextualizationController* GetTabContextualizationController() {
    tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(
        browser()->tab_strip_model()->GetWebContentsAt(0));
    return static_cast<TestTabContextualizationController*>(
        TabContextualizationController::From(tab));
  }

 private:
  ui::UserDataFactory::ScopedOverride controller_override_;
};

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       EligibilityIsTrueForEligiblePage) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::test::TestFuture<void> future;
  controller->UpdatePageContextEligibility(
      base::BindOnce(&TabContextualizationController::OnEligibilityChecked,
                     base::Unretained(controller))
          .Then(future.GetCallback()));
  EXPECT_TRUE(future.Wait());

  EXPECT_TRUE(controller->GetCurrentPageContextEligibility());
}

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       EligibilityIsFalseForIneligiblePage) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::test::TestFuture<void> future;
  controller->UpdatePageContextEligibility(
      base::BindOnce(&TabContextualizationController::OnEligibilityChecked,
                     base::Unretained(controller))
          .Then(future.GetCallback()));
  EXPECT_TRUE(future.Wait());

  controller->OnEligibilityChecked(false,
                                   base::unexpected("Uninitialized APC"));

  EXPECT_FALSE(controller->GetCurrentPageContextEligibility());
}

#if BUILDFLAG(ENABLE_PDF)
IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       GetPageContextForPdf) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Get the viewport size to compare with the screenshot.
  gfx::Size viewport_size = GetViewportPhysicalSize(browser());

  content::RenderFrameHost* primary_main_frame =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame();
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(primary_main_frame));

  base::test::TestFuture<std::unique_ptr<lens::ContextualInputData>> future;
  controller->GetPageContext(future.GetCallback());
  auto data = future.Take();

  EXPECT_TRUE(data->tab_session_id.has_value());
  EXPECT_EQ(data->page_url, url);
  EXPECT_TRUE(data->page_title.has_value());
  EXPECT_TRUE(data->pdf_current_page.has_value());
  EXPECT_EQ(data->primary_content_type, lens::MimeType::kPdf);
  EXPECT_EQ(data->context_input->size(), 1u);
  EXPECT_EQ(data->context_input->at(0).content_type_, lens::MimeType::kPdf);
  EXPECT_FALSE(data->context_input->at(0).bytes_.empty());

  EXPECT_TRUE(data->viewport_screenshot.has_value());
  EXPECT_FALSE(data->viewport_screenshot->drawsNothing());
  EXPECT_EQ(data->viewport_screenshot->width(), viewport_size.width());
  EXPECT_EQ(data->viewport_screenshot->height(), viewport_size.height());

  EXPECT_TRUE(data->is_page_context_eligible.has_value());
  EXPECT_TRUE(data->is_page_context_eligible.value());
}

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       GetPageContextForIneligiblePdf) {
  auto* controller = GetTabContextualizationController();
  controller->SetIsPageContextEligible(false);

  GURL url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* primary_main_frame =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame();
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(primary_main_frame));

  base::test::TestFuture<std::unique_ptr<lens::ContextualInputData>> future;
  controller->GetPageContext(future.GetCallback());
  auto data = future.Take();

  EXPECT_EQ(data->primary_content_type, lens::MimeType::kPdf);
  EXPECT_TRUE(data->is_page_context_eligible.has_value());
  EXPECT_FALSE(data->is_page_context_eligible.value());
}
#endif  // BUILDFLAG(ENABLE_PDF)

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       GetPageContextForWebpage) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Get the viewport size to compare with the screenshot.
  gfx::Size viewport_size = GetViewportPhysicalSize(browser());

  base::test::TestFuture<std::unique_ptr<lens::ContextualInputData>> future;
  controller->GetPageContext(future.GetCallback());
  auto data = future.Take();

  EXPECT_TRUE(data->tab_session_id.has_value());
  EXPECT_EQ(data->page_url, url);
  EXPECT_TRUE(data->page_title.has_value());
  EXPECT_EQ(data->primary_content_type, lens::MimeType::kAnnotatedPageContent);
  EXPECT_EQ(data->context_input->size(), 1u);
  EXPECT_EQ(data->context_input->at(0).content_type_,
            lens::MimeType::kAnnotatedPageContent);
  EXPECT_FALSE(data->context_input->at(0).bytes_.empty());

  EXPECT_TRUE(data->is_page_context_eligible);

  EXPECT_TRUE(data->viewport_screenshot.has_value());
  EXPECT_FALSE(data->viewport_screenshot->drawsNothing());
  EXPECT_EQ(data->viewport_screenshot->width(), viewport_size.width());
  EXPECT_EQ(data->viewport_screenshot->height(), viewport_size.height());
}

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       CaptureScreenshot) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Get the viewport size to compare with the screenshot.
  gfx::Size viewport_size = GetViewportPhysicalSize(browser());

  base::test::TestFuture<const SkBitmap&> future;
  controller->CaptureScreenshot(/*image_options=*/std::nullopt,
                                future.GetCallback());
  auto screenshot = future.Get();

  EXPECT_FALSE(screenshot.drawsNothing());
  EXPECT_EQ(screenshot.width(), viewport_size.width());
  EXPECT_EQ(screenshot.height(), viewport_size.height());
}

IN_PROC_BROWSER_TEST_F(TabContextualizationControllerBrowserTest,
                       CaptureScreenshotWithImageOptions) {
  auto* controller = GetTabContextualizationController();

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Downscale the screenshot to half of the viewport size.
  gfx::Size viewport_size = GetViewportPhysicalSize(browser());
  lens::ImageEncodingOptions image_options{
      .max_height = viewport_size.height() / 2,
      .max_width = viewport_size.width() / 2};

  base::test::TestFuture<const SkBitmap&> future;
  controller->CaptureScreenshot(image_options, future.GetCallback());
  auto screenshot = future.Get();

  EXPECT_FALSE(screenshot.drawsNothing());
  EXPECT_LE(screenshot.width(), image_options.max_width);
  EXPECT_LE(screenshot.height(), image_options.max_height);
}

}  // namespace lens
