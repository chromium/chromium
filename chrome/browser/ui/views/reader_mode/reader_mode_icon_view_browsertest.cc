// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "chrome/browser/ui/views/reader_mode/reader_mode_icon_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/content/browser/test_distillability_observer.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/test/button_test_api.h"

namespace {

const char* kSimpleArticlePath = "/dom_distiller/simple_article.html";
const char* kNonArticlePath = "/dom_distiller/non_og_article.html";
const char* kArticleTitle = "Test Page Title";

class ReaderModeIconViewBrowserTest : public InProcessBrowserTest {
 public:
  ReaderModeIconViewBrowserTest(const ReaderModeIconViewBrowserTest&) = delete;
  ReaderModeIconViewBrowserTest& operator=(
      const ReaderModeIconViewBrowserTest&) = delete;

 protected:
  ReaderModeIconViewBrowserTest() {
    feature_list_.InitAndEnableFeature(dom_distiller::kReaderMode);
    https_server_secure_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_secure_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(https_server_secure()->Start());
    reader_mode_icon_ =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kReaderMode);
    ASSERT_NE(nullptr, reader_mode_icon_);
  }

  net::EmbeddedTestServer* https_server_secure() {
    return https_server_secure_.get();
  }

  raw_ptr<PageActionIconView, DanglingUntriaged> reader_mode_icon_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_secure_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(gilmanmh): Add tests for the following cases:
//  * Icon is visible on the distilled page.
//  * Icon is not visible on about://blank, both initially and after navigating
//    to a distillable page.
IN_PROC_BROWSER_TEST_F(ReaderModeIconViewBrowserTest,
                       IconVisibilityAdaptsToPageContents) {
  dom_distiller::TestDistillabilityObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  dom_distiller::DistillabilityResult expected_result;
  expected_result.is_distillable = false;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  // The icon should not be visible by default, before navigation to any page
  // has occurred.
  const bool is_visible_before_navigation = reader_mode_icon_->GetVisible();
  EXPECT_FALSE(is_visible_before_navigation);

  // The icon should be hidden on pages that aren't distillable
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kNonArticlePath)));
  observer.WaitForResult(expected_result);
  const bool is_visible_on_non_distillable_page =
      reader_mode_icon_->GetVisible();
  EXPECT_FALSE(is_visible_on_non_distillable_page);
  EXPECT_FALSE(observer.IsDistillabilityDriverTimerRunning());

  // The icon should appear after navigating to a distillable article.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kSimpleArticlePath)));
  expected_result.is_distillable = true;
  observer.WaitForResult(expected_result);
  const bool is_visible_on_article = reader_mode_icon_->GetVisible();
  EXPECT_TRUE(is_visible_on_article);
  EXPECT_TRUE(observer.IsDistillabilityDriverTimerRunning());

  // Navigating back to a non-distillable page hides the icon again.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kNonArticlePath)));
  expected_result.is_distillable = false;
  observer.WaitForResult(expected_result);
  const bool is_visible_after_navigation_back_to_non_distillable_page =
      reader_mode_icon_->GetVisible();
  EXPECT_FALSE(is_visible_after_navigation_back_to_non_distillable_page);
}

class ReaderModeIconViewPrerenderBrowserTest
    : public ReaderModeIconViewBrowserTest {
 public:
  ReaderModeIconViewPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ReaderModeIconViewPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~ReaderModeIconViewPrerenderBrowserTest() override = default;

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ReaderModeIconViewPrerenderBrowserTest,
                       IconVisibilityNotAffectedByPrerendering) {
  dom_distiller::TestDistillabilityObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  dom_distiller::DistillabilityResult expected_result;
  expected_result.is_distillable = false;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  // The icon should not be visible by default, before navigation to any page
  // has occurred.
  const bool is_visible_before_navigation = reader_mode_icon_->GetVisible();
  EXPECT_FALSE(is_visible_before_navigation);

  // The icon should be hidden on pages that aren't distillable.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kNonArticlePath)));
  observer.WaitForResult(expected_result);
  const bool is_visible_on_non_distillable_page =
      reader_mode_icon_->GetVisible();
  EXPECT_FALSE(is_visible_on_non_distillable_page);

  // The icon should not yet appear after prerendering a distillable article.
  prerender_helper_.AddPrerender(
      https_server_secure()->GetURL(kSimpleArticlePath));
  EXPECT_FALSE(observer.IsDistillabilityDriverTimerRunning());

  // Make the prerendering page primary (i.e. the user clicked on a link to
  // the prerendered URL). The icon should become visible at this point.
  prerender_helper_.NavigatePrimaryPage(
      https_server_secure()->GetURL(kSimpleArticlePath));
  expected_result.is_distillable = true;
  observer.WaitForResult(expected_result);
  const bool is_visible_on_article = reader_mode_icon_->GetVisible();
  EXPECT_TRUE(is_visible_on_article);
  EXPECT_TRUE(observer.IsDistillabilityDriverTimerRunning());
}

IN_PROC_BROWSER_TEST_F(ReaderModeIconViewPrerenderBrowserTest,
                       InkDropStateNotAffectedByPrerendering) {
  dom_distiller::TestDistillabilityObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  dom_distiller::DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  // Navigate to a distillable page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kSimpleArticlePath)));
  observer.WaitForResult(expected_result);

  // The icon should be visible for the distillable page.
  EXPECT_TRUE(reader_mode_icon_->GetVisible());

  // Force the ink drop to activate for testing.
  views::InkDrop::Get(reader_mode_icon_)
      ->AnimateToState(views::InkDropState::ACTIVATED, nullptr);

  // Prerender a page that is not distillable.
  prerender_helper_.AddPrerender(
      https_server_secure()->GetURL(kNonArticlePath));

  // Prerendering does not affect the visibility of the icon and the ink drop
  // state. So the icon should be still visible and the ink drop should be still
  // activated.
  EXPECT_TRUE(reader_mode_icon_->GetVisible());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            views::InkDrop::Get(reader_mode_icon_)
                ->GetInkDrop()
                ->GetTargetInkDropState());

  // Make the prerendering page primary.
  prerender_helper_.NavigatePrimaryPage(
      https_server_secure()->GetURL(kNonArticlePath));
  expected_result.is_distillable = false;
  observer.WaitForResult(expected_result);

  // The new primary page is not distillable. The icon should be invisible and
  // the ink drop should be hidden.
  EXPECT_FALSE(reader_mode_icon_->GetVisible());
  EXPECT_EQ(views::InkDropState::HIDDEN, views::InkDrop::Get(reader_mode_icon_)
                                             ->GetInkDrop()
                                             ->GetTargetInkDropState());
}

class ReaderModeIconViewBrowserTestWithSettings
    : public ReaderModeIconViewBrowserTest {
 public:
  ReaderModeIconViewBrowserTestWithSettings(
      const ReaderModeIconViewBrowserTestWithSettings&) = delete;
  ReaderModeIconViewBrowserTestWithSettings& operator=(
      const ReaderModeIconViewBrowserTestWithSettings&) = delete;

 protected:
  ReaderModeIconViewBrowserTestWithSettings() {
    feature_list_.InitAndEnableFeatureWithParameters(
        dom_distiller::kReaderMode,
        {{switches::kReaderModeDiscoverabilityParamName,
          switches::kReaderModeOfferInSettings}});
  }

  void SetOfferReaderModeSetting(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        dom_distiller::prefs::kOfferReaderMode, value);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Flaky on Linux Win ChromeOS: crbug.com/1054641
IN_PROC_BROWSER_TEST_F(
    ReaderModeIconViewBrowserTestWithSettings,
    DISABLED_IconVisibilityDependsOnSettingIfExperimentEnabled) {
  SetOfferReaderModeSetting(false);

  dom_distiller::TestDistillabilityObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  dom_distiller::DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  // The icon should not appear after navigating to a distillable article,
  // because the setting to offer reader mode is disabled.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kSimpleArticlePath)));
  observer.WaitForResult(expected_result);
  bool is_visible_on_article = reader_mode_icon_->GetVisible();
  EXPECT_FALSE(is_visible_on_article);

  // It continues to not show up when navigating to a non-distillable page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kNonArticlePath)));
  expected_result.is_distillable = false;
  observer.WaitForResult(expected_result);
  bool is_visible_after_navigation_back_to_non_distillable_page =
      reader_mode_icon_->GetVisible();
  EXPECT_FALSE(is_visible_after_navigation_back_to_non_distillable_page);

  // If we turn on the setting, the icon should start to show up on a
  // distillable page.
  SetOfferReaderModeSetting(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kSimpleArticlePath)));
  expected_result.is_distillable = true;
  observer.WaitForResult(expected_result);
  is_visible_on_article = reader_mode_icon_->GetVisible();
  EXPECT_TRUE(is_visible_on_article);

  // But it still turns off when navigating to a non-distillable page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kNonArticlePath)));
  expected_result.is_distillable = false;
  observer.WaitForResult(expected_result);
  is_visible_after_navigation_back_to_non_distillable_page =
      reader_mode_icon_->GetVisible();
  EXPECT_FALSE(is_visible_after_navigation_back_to_non_distillable_page);
}

IN_PROC_BROWSER_TEST_F(ReaderModeIconViewBrowserTest,
                       NonSecurePagesNotDistillable) {
  auto https_server_expired = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired->SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);

  dom_distiller::TestDistillabilityObserver observer(web_contents);
  dom_distiller::DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  // Check test setup by ensuring the icon is shown with the secure test
  // server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_secure()->GetURL(kSimpleArticlePath)));
  EXPECT_EQ(security_state::SECURE, helper->GetSecurityLevel());
  observer.WaitForResult(expected_result);
  EXPECT_TRUE(reader_mode_icon_->GetVisible());

  // The icon should not be shown with a http test server.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSimpleArticlePath)));
  EXPECT_NE(security_state::SECURE, helper->GetSecurityLevel());
  expected_result.is_distillable = false;
  observer.WaitForResult(expected_result);
  EXPECT_FALSE(reader_mode_icon_->GetVisible());

  // Set security state to DANGEROUS for the test by using an expired
  // certificate.
  ASSERT_TRUE(https_server_expired->Start());
  content::TitleWatcher title_watcher(web_contents,
                                      base::ASCIIToUTF16(kArticleTitle));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_expired->GetURL(kSimpleArticlePath)));

  // Proceed through the intersitial warning page.
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents, 1);
  security_interstitials::SecurityInterstitialTabHelper* interstitial_helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          web_contents);
  interstitial_helper
      ->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
      ->CommandReceived(
          base::NumberToString(security_interstitials::CMD_PROCEED));
  nav_observer.Wait();

  // Check we are on the right page.
  ASSERT_EQ(base::ASCIIToUTF16(kArticleTitle), title_watcher.WaitAndGetTitle());

  // Check security state is DANGEROUS per https_server_expired_.
  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());

  // The page should not be distillable.
  observer.WaitForResult(expected_result);
  EXPECT_FALSE(reader_mode_icon_->GetVisible());
}

}  // namespace
