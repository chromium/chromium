// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_core_tab_side_panel_helper.h"

#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {
class LensCoreTabSidePanelHelperBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    // The test server must start first, so that we know the port that the test
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUp();
  }

  GURL GetImageSearchURL() {
    constexpr char kImageSearchURL[] = "/imagesearch?p=somepayload";
    return embedded_test_server()->GetURL(kImageSearchURL);
  }

  void SetGoogleToOptOutOfSidePanel() {
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
    TemplateURLData data;
    data.side_image_search_param = "";
    // This should trigger DefaultSearchProviderIsGoogle to true.
    data.SetURL("http://www.google.com/");

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void Setup3PImageSearchEngine(bool support_side_panel) {
    constexpr char kSearchURL[] = "/search?q={searchTerms}";
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.SetURL(embedded_test_server()->GetURL(kSearchURL).spec());
    data.side_image_search_param = support_side_panel ? "sideimagesearch" : "";

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  content::WebContents* GetBrowserWebContents() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetActiveWebContents();
  }

  content::WebContents* GetPwaWebContents() {
    // Creates a Progressive Web App and set it to the default browser
    Browser* pwa_browser = InProcessBrowserTest::CreateBrowserForApp(
        "test_app", browser()->profile());
    CloseBrowserSynchronously(browser());
    SelectFirstBrowser();

    EXPECT_TRUE(pwa_browser->is_type_app());

    return BrowserView::GetBrowserViewForBrowser(pwa_browser)
        ->GetActiveWebContents();
  }
};

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelperBrowserTest,
                       IsSidePanelEnabledForLensReturnsFalseForAndroid) {
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(web_contents));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
IN_PROC_BROWSER_TEST_F(
    LensCoreTabSidePanelHelperBrowserTest,
    IsSidePanelEnabledForLensReturnsFalseIfGoogleBrandedFeaturesAreDisabled) {
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(web_contents));
}
#endif  // !BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
//  IsSidePanelEnabledForLens
IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelperBrowserTest,
                       IsSidePanelEnabledForLensReturnsFalseForPwa) {
  auto* pwa_web_contents = GetPwaWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(pwa_web_contents));
}

IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelperBrowserTest,
                       IsSidePanelEnabledForLensReturnsTrueForBrowser) {
  auto* web_contents = GetBrowserWebContents();

  EXPECT_TRUE(lens::IsSidePanelEnabledForLens(web_contents));
}

IN_PROC_BROWSER_TEST_F(
    LensCoreTabSidePanelHelperBrowserTest,
    IsSidePanelEnabledForLensReturnsFalseForBrowserIfOptedOutInTemplateUrl) {
  SetGoogleToOptOutOfSidePanel();
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(web_contents));
}

IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelperBrowserTest,
                       IsSidePanelEnabledForLensReturnsFalseFor3PDse) {
  Setup3PImageSearchEngine(/*support_side_panel*/ true);
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(web_contents));
}
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

class LensCoreTabSidePanelHelperLensStandaloneDisabled
    : public LensCoreTabSidePanelHelperBrowserTest {
 protected:
  void SetUp() override {
    // The test server must start first, so that we know the port that the
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{}, {}}, {{lens::features::kLensStandalone}});
    InProcessBrowserTest::SetUp();
  }
};

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelperLensStandaloneDisabled,
                       IsSidePanelEnabledForLensReturnsFalseForAndroid) {
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(web_contents));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
IN_PROC_BROWSER_TEST_F(
    LensCoreTabSidePanelHelperLensStandaloneDisabled,
    IsSidePanelEnabledForLensReturnsFalseIfGoogleBrandedFeaturesAreDisabled) {
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(web_contents));
}
#endif  // !BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelperLensStandaloneDisabled,
                       IsSidePanelEnabledForLensReturnsFalse) {
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(web_contents));
}

IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelperLensStandaloneDisabled,
                       IsSidePanelEnabledForLensReturnsFalseForPwa) {
  auto* web_contents = GetPwaWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledForLens(web_contents));
}
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

class LensCoreTabSidePanelHelperRegionSearchBrowserTest
    : public LensCoreTabSidePanelHelperBrowserTest {
 protected:
  void SetUp() override {
    // The test server must start first, so that we know the port that the
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {
            {lens::features::kLensStandalone,
             {{lens::features::kHomepageURLForLens.name,
               GetLensRegionSearchURL().spec()}}},

        },
        {});
    InProcessBrowserTest::SetUp();
  }

  GURL GetLensRegionSearchURL() {
    static const std::string kLensRegionSearchURL =
        lens::features::GetHomepageURLForLens() + "upload?ep=crs";
    return GURL(kLensRegionSearchURL);
  }
};

class LensCoreTabSidePanelHelper3PDseEnabled
    : public LensCoreTabSidePanelHelperBrowserTest {
 protected:
  void SetUp() override {
    // The test server must start first, so that we know the port that the
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());

    base::test::ScopedFeatureList features;
    features.InitWithFeaturesAndParameters(
        {{lens::features::kEnableImageSearchSidePanelFor3PDse, {{}}}}, {});

    InProcessBrowserTest::SetUp();
  }
};

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelper3PDseEnabled,
                       IsSidePanelEnabledFor3PDseReturnsFalseForAndroid) {
  Setup3PImageSearchEngine(/*support_side_panel*/ true);
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledFor3PDse(web_contents));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
IN_PROC_BROWSER_TEST_F(
    LensCoreTabSidePanelHelper3PDseEnabled,
    IsSidePanelEnabledFor3PDseReturnsFalseIfGoogleBrandedFeaturesAreDisabled) {
  Setup3PImageSearchEngine(/*support_side_panel*/ true);
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledFor3PDse(web_contents));
}
#endif  // !BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

//  IsSidePanelEnabledFor3PDse
IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelper3PDseEnabled,
                       IsSidePanelEnabledFor3PDseReturnsTrue) {
  Setup3PImageSearchEngine(/*support_side_panel*/ true);
  auto* web_contents = GetBrowserWebContents();

  EXPECT_TRUE(lens::IsSidePanelEnabledFor3PDse(web_contents));
}

IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelper3PDseEnabled,
                       IsSidePanelEnabledFor3PDseReturnsFalseIf3POptedOut) {
  Setup3PImageSearchEngine(/*support_side_panel*/ false);
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledFor3PDse(web_contents));
}

IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelper3PDseEnabled,
                       IsSidePanelEnabledFor3PDseReturnsFalseForPwa) {
  Setup3PImageSearchEngine(/*support_side_panel*/ true);
  auto* pwa_web_contents = GetPwaWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledFor3PDse(pwa_web_contents));
}

IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelper3PDseEnabled,
                       IsSidePanelEnabledFor3PDseReturnsFalseForGoogle) {
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledFor3PDse(web_contents));
}
class LensCoreTabSidePanelHelper3PDseDisabledUsingFlag
    : public LensCoreTabSidePanelHelperBrowserTest {
 protected:
  void SetUp() override {
    // The test server must start first, so that we know the port that the
    // server is using.
    ASSERT_TRUE(embedded_test_server()->Start());

    base::test::ScopedFeatureList features;
    features.InitWithFeaturesAndParameters(
        {{}, {}}, {{lens::features::kEnableImageSearchSidePanelFor3PDse}});

    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(LensCoreTabSidePanelHelper3PDseDisabledUsingFlag,
                       IsSidePanelEnabledFor3PDseReturnsFalse) {
  Setup3PImageSearchEngine(/*support_side_panel*/ true);
  auto* web_contents = GetBrowserWebContents();

  EXPECT_FALSE(lens::IsSidePanelEnabledFor3PDse(web_contents));
}
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
}  // namespace lens
