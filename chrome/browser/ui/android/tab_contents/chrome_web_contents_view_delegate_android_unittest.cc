// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_contents/chrome_web_contents_view_delegate_android.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/testing_search_terms_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation.h"
#include "url/gurl.h"

class ChromeWebContentsViewDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  // Provide the factories needed for TemplateURLService to function.
  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories;
    factories.push_back(
        {TemplateURLServiceFactory::GetInstance(),
         TemplateURLServiceTestUtil::GetTemplateURLServiceTestingFactory()});
    return factories;
  }

  void SetIsSrp(const GURL& url) {
    auto* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    // Standard SRP pattern: /search?q=...
    data.SetURL(url.GetWithEmptyPath().spec() + "search?q={searchTerms}");
    auto* turl = template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(turl);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Force animations ON for these tests, as some CI bots have them disabled.
    gfx::Animation::SetPrefersReducedMotionForTesting(false);

    // TemplateURLService must be loaded before it can be used.
    auto* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    template_url_service->Load();
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    delegate_ =
        std::make_unique<ChromeWebContentsViewDelegateAndroid>(web_contents());
  }

 protected:
  const GURL kSrpUrl{"https://www.google.com/search?q=test"};
  const GURL kNonSrpUrl{"https://www.example.com"};
  const GURL kReaderModeUrl{"chrome-distiller://example-id"};

  std::unique_ptr<ChromeWebContentsViewDelegateAndroid> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeWebContentsViewDelegateAndroidTest,
       ShouldShowBlurTransition_GenericEnabled_NoSkipSrp) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kAndroidNavigationBlurTransitionAnimation,
      {{"skip_srp", "false"}});

  // Animation should be shown for any navigation.
  content::MockNavigationHandle srp_handle(kSrpUrl, main_rfh());
  EXPECT_TRUE(delegate_->ShouldShowBlurTransitionAnimation(&srp_handle));

  content::MockNavigationHandle non_srp_handle(kNonSrpUrl, main_rfh());
  EXPECT_TRUE(delegate_->ShouldShowBlurTransitionAnimation(&non_srp_handle));
}

TEST_F(ChromeWebContentsViewDelegateAndroidTest,
       ShouldShowBlurTransition_GenericEnabled_SkipSrp_NotSrp) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::
                                 kAndroidNavigationBlurTransitionAnimation,
                             {{"skip_srp", "true"}}}},
      /*disabled_features=*/{
          dom_distiller::kReaderModeBlurTransitionAnimation});

  // Not SRP -> Animation should be shown.
  SetIsSrp(kSrpUrl);
  content::MockNavigationHandle handle(kNonSrpUrl, main_rfh());
  EXPECT_TRUE(delegate_->ShouldShowBlurTransitionAnimation(&handle));
}

TEST_F(ChromeWebContentsViewDelegateAndroidTest,
       ShouldShowBlurTransition_GenericEnabled_SkipSrp_IsSrp) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::
                                 kAndroidNavigationBlurTransitionAnimation,
                             {{"skip_srp", "true"}}}},
      /*disabled_features=*/{
          dom_distiller::kReaderModeBlurTransitionAnimation});

  // Is SRP -> Animation should NOT be shown.
  SetIsSrp(kSrpUrl);
  content::MockNavigationHandle handle(kSrpUrl, main_rfh());
  EXPECT_FALSE(delegate_->ShouldShowBlurTransitionAnimation(&handle));
}

TEST_F(
    ChromeWebContentsViewDelegateAndroidTest,
    ShouldShowBlurTransition_GenericEnabled_SkipSrp_IsSrp_ReaderModeDisabled) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::
                                 kAndroidNavigationBlurTransitionAnimation,
                             {{"skip_srp", "true"}}}},
      /*disabled_features=*/{
          dom_distiller::kReaderModeBlurTransitionAnimation});

  // SRP to Reader Mode transition. Reader Mode feature is disabled.
  SetIsSrp(kSrpUrl);
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(kSrpUrl);
  content::MockNavigationHandle handle(kReaderModeUrl, main_rfh());

  // Animation should NOT be shown.
  EXPECT_FALSE(delegate_->ShouldShowBlurTransitionAnimation(&handle));
}

TEST_F(
    ChromeWebContentsViewDelegateAndroidTest,
    ShouldShowBlurTransition_GenericEnabled_SkipSrp_IsSrp_ReaderModeEnabled) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::
                                 kAndroidNavigationBlurTransitionAnimation,
                             {{"skip_srp", "true"}}},
                            {dom_distiller::kReaderModeBlurTransitionAnimation,
                             {}}},
      /*disabled_features=*/{});

  // SRP to Reader Mode transition. Reader Mode feature is enabled.
  SetIsSrp(kSrpUrl);
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(kSrpUrl);
  content::MockNavigationHandle handle(kReaderModeUrl, main_rfh());

  // Animation should be shown.
  EXPECT_TRUE(delegate_->ShouldShowBlurTransitionAnimation(&handle));
}

TEST_F(ChromeWebContentsViewDelegateAndroidTest,
       ShouldShowBlurTransition_GenericDisabled_ReaderModeEnabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{dom_distiller::kReaderModeBlurTransitionAnimation},
      /*disabled_features=*/{
          features::kAndroidNavigationBlurTransitionAnimation});

  // Is Reader Mode -> Animation should be shown.
  content::MockNavigationHandle reader_handle(kReaderModeUrl, main_rfh());
  EXPECT_TRUE(delegate_->ShouldShowBlurTransitionAnimation(&reader_handle));

  // Not Reader Mode -> Animation should NOT be shown.
  content::MockNavigationHandle non_reader_handle(kNonSrpUrl, main_rfh());
  EXPECT_FALSE(
      delegate_->ShouldShowBlurTransitionAnimation(&non_reader_handle));
}

TEST_F(ChromeWebContentsViewDelegateAndroidTest,
       ShouldShowBlurTransition_GenericDisabled_ReaderModeDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          features::kAndroidNavigationBlurTransitionAnimation,
          dom_distiller::kReaderModeBlurTransitionAnimation});

  content::MockNavigationHandle handle(kReaderModeUrl, main_rfh());
  EXPECT_FALSE(delegate_->ShouldShowBlurTransitionAnimation(&handle));
}
