// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/lens/lens_features.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

// Lens ping URL for tests.
const char kTestLensPingURL[] = "https://lens-ping.url/";

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_OptimizedImageFormatsDisabled_EncodesAsPng) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(lens::features::kLensImageFormatOptimizations);
  gfx::Image image = gfx::test::CreateImage(100, 100);
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_FALSE(search_args.image_thumbnail_content.empty());
  EXPECT_EQ("image/png", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::PNG, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_WebpEnabledAndEncodingSucceeds_EncodesAsWebp) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kLensImageFormatOptimizations,
      {{"use-webp-region-search", "true"},
       {"use-jpeg-region-search", "false"}});
  gfx::Image image = gfx::test::CreateImage(100, 100);
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_FALSE(search_args.image_thumbnail_content.empty());
  EXPECT_EQ("image/webp", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::WEBP, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_WebpEnabledAndEncodingFails_EncodesAsPng) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kLensImageFormatOptimizations,
      {{"use-webp-region-search", "true"},
       {"use-jpeg-region-search", "false"}});
  gfx::Image image = gfx::test::CreateImage(0, 0);  // Encoding 0x0 will fail
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_EQ("image/png", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::PNG, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_JpegEnabledAndEncodingSucceeds_EncodesAsJpeg) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kLensImageFormatOptimizations,
      {{"use-webp-region-search", "false"},
       {"use-jpeg-region-search", "true"}});
  gfx::Image image = gfx::test::CreateImage(100, 100);
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_FALSE(search_args.image_thumbnail_content.empty());
  EXPECT_EQ("image/jpeg", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::JPEG, image_format);
}

TEST(CoreTabHelperUnitTest,
     EncodeImageIntoSearchArgs_JpegEnabledAndEncodingFails_EncodesAsPng) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kLensImageFormatOptimizations,
      {{"use-webp-region-search", "false"},
       {"use-jpeg-region-search", "true"}});
  gfx::Image image = gfx::test::CreateImage(0, 0);  // Encoding 0x0 will fail
  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  lens::mojom::ImageFormat image_format =
      CoreTabHelper::EncodeImageIntoSearchArgs(image, search_args);

  EXPECT_EQ("image/png", search_args.image_thumbnail_content_type);
  EXPECT_EQ(lens::mojom::ImageFormat::PNG, image_format);
}

class CoreTabHelperWindowUnitTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://www.google.com/"));

    CoreTabHelper::CreateForWebContents(web_contents());
    core_tab_helper_ = CoreTabHelper::FromWebContents(web_contents());

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service_);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  content::RenderFrameHost* main_rfh() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  content::NavigationController* navigation_controller() {
    return &(web_contents()->GetController());
  }

 private:
  raw_ptr<CoreTabHelper> core_tab_helper_;
  raw_ptr<TemplateURLService> template_url_service_;
};

TEST_F(CoreTabHelperWindowUnitTest,
       SearchWithLens_LensPingEnabled_TriggersLensPing) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      lens::features::kEnableLensPing, {{"ping-lens-sequentially", "true"},
                                        {"lens-ping-url", kTestLensPingURL}});

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(web_contents());
  core_tab_helper->SearchWithLens(
      main_rfh(), GURL(""),
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM,
      false);
  EXPECT_TRUE(core_tab_helper->awaiting_lens_ping_response_);

  EXPECT_EQ(kTestLensPingURL,
            navigation_controller()->GetVisibleEntry()->GetURL().spec());

  // Trigger the DidFinishNavigation callback of WebContentsObserver with a
  // simulated Lens ping response. Purposely do not set the handle committed
  // flag, as the Lens ping should have a 204 response code.
  content::MockNavigationHandle simulated_lens_ping_handle(
      GURL(kTestLensPingURL), main_rfh());
  core_tab_helper->DidFinishNavigation(&simulated_lens_ping_handle);
  EXPECT_FALSE(core_tab_helper->awaiting_lens_ping_response_);
}
