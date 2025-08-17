// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {
const char kNonNtpUrl[] = "https://www.google.com";
}

class FooterControllerExtensionTestBase
    : public extensions::ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
    browser()
        ->GetFeatures()
        .new_tab_footer_controller()
        ->SkipErrorPageCheckForTesting(true);
  }

  scoped_refptr<const extensions::Extension> LoadNtpExtension() {
    extensions::TestExtensionDir extension_dir;
    constexpr char kManifest[] = R"(
                            {
                              "chrome_url_overrides": {
                                  "newtab": "ext.html"
                              },
                              "name": "Extension-overridden NTP",
                              "manifest_version": 3,
                              "version": "0.1"
                            })";
    extension_dir.WriteManifest(kManifest);
    extension_dir.WriteFile(FILE_PATH_LITERAL("ext.html"),
                            "<body>Extension-overridden NTP</body>");
    scoped_refptr<const extensions::Extension> extension =
        LoadExtension(extension_dir.Pack());
    return extension;
  }

  void NavigateCurrentTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void OpenNewTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void VerifyNoticeMetricsRecorded(int total_count,
                                   int management_count = 0,
                                   int extension_count = 0) {
    const std::string& notice_item = "NewTabPage.Footer.NoticeItem";
    histogram_tester_.ExpectTotalCount(notice_item, total_count);
    histogram_tester_.ExpectBucketCount(
        notice_item, new_tab_footer::FooterNoticeItem::kManagementNotice,
        management_count);
    histogram_tester_.ExpectBucketCount(
        notice_item, new_tab_footer::FooterNoticeItem::kExtensionAttribution,
        extension_count);
  }

  void TestUserPrefChanged() {
    profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
    auto extension = LoadNtpExtension();
    NavigateCurrentTab(extension->url());
    ASSERT_FALSE(footer()->GetVisible());

    profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
    EXPECT_TRUE(footer()->GetVisible());

    profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
    EXPECT_FALSE(footer()->GetVisible());
  }

  void TestAttributionPolicyChanged() {
    auto extension = LoadNtpExtension();
    ASSERT_FALSE(footer()->GetVisible());

    NavigateCurrentTab(extension->url());
    EXPECT_TRUE(footer()->GetVisible());

    profile()->GetPrefs()->SetBoolean(
        prefs::kNTPFooterExtensionAttributionEnabled, false);
    EXPECT_FALSE(footer()->GetVisible());

    profile()->GetPrefs()->SetBoolean(
        prefs::kNTPFooterExtensionAttributionEnabled, true);
    EXPECT_TRUE(footer()->GetVisible());
  }

  void TestMetricsRecorded() {
    const std::string& visible_on_load = "NewTabPage.Footer.VisibleOnLoad";

    auto extension = LoadNtpExtension();
    histogram_tester_.ExpectTotalCount(visible_on_load, 0);
    VerifyNoticeMetricsRecorded(0);

    NavigateCurrentTab(extension->url());
    histogram_tester_.ExpectTotalCount(visible_on_load, 1);
    histogram_tester_.ExpectBucketCount(visible_on_load, true, 1);
    VerifyNoticeMetricsRecorded(/*total_count= */ 1, /*management_count= */ 0,
                                /*extension_count= */ 1);

    profile()->GetPrefs()->SetBoolean(
        prefs::kNTPFooterExtensionAttributionEnabled, false);
    histogram_tester_.ExpectTotalCount(visible_on_load, 1);
    histogram_tester_.ExpectBucketCount(visible_on_load, true, 1);

    NavigateCurrentTab(extension->url());
    histogram_tester_.ExpectTotalCount(visible_on_load, 2);
    histogram_tester_.ExpectBucketCount(visible_on_load, true, 1);
    histogram_tester_.ExpectBucketCount(visible_on_load, false, 1);
    VerifyNoticeMetricsRecorded(/*total_count= */ 1, /*management_count= */ 0,
                                /*extension_count= */ 1);
  }

  void TestShownTimeRecorded() {
    const std::string& shown_time = "NewTabPage.Footer.ShownTime";

    auto extension = LoadNtpExtension();
    histogram_tester_.ExpectTotalCount(shown_time, 0);

    base::TimeTicks start = base::TimeTicks::Now();
    NavigateCurrentTab(extension->url());
    int max_expected = (base::TimeTicks::Now() - start).InMilliseconds();

    histogram_tester_.ExpectTotalCount(shown_time, 1);
    int actual = histogram_tester_.GetAllSamples(shown_time)[0].min;
    EXPECT_GT(actual, 1);
    EXPECT_LE(actual, max_expected);
  }

  new_tab_footer::NewTabFooterWebView* footer() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetActiveContentsContainerView()
        ->new_tab_footer_view();
  }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

class FooterControllerExtensionTest : public FooterControllerExtensionTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  FooterControllerExtensionTest() {
    feature_list_.InitWithFeatureStates(
        {{ntp_features::kNtpFooter, true},
         {features::kSideBySide, side_by_side_enabled()}});
  }
  ~FooterControllerExtensionTest() override = default;

  bool side_by_side_enabled() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(, FooterControllerExtensionTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(FooterControllerExtensionTest, TabChanged) {
  ASSERT_FALSE(footer()->GetVisible());

  auto extension = LoadNtpExtension();
  ASSERT_FALSE(footer()->GetVisible());

  OpenNewTab(GURL(extension->url()));
  EXPECT_TRUE(footer()->GetVisible());

  NavigateCurrentTab(GURL(kNonNtpUrl));
  EXPECT_FALSE(footer()->GetVisible());

  NavigateCurrentTab(extension->url());
  EXPECT_TRUE(footer()->GetVisible());

  OpenNewTab(GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_FALSE(footer()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(FooterControllerExtensionTest, UserPrefChanged) {
  TestUserPrefChanged();
}

IN_PROC_BROWSER_TEST_P(FooterControllerExtensionTest,
                       AttributionPolicyChanged) {
  TestAttributionPolicyChanged();
}

IN_PROC_BROWSER_TEST_P(FooterControllerExtensionTest, MetricsRecorded) {
  TestMetricsRecorded();
}

IN_PROC_BROWSER_TEST_P(FooterControllerExtensionTest, ShownTimeRecorded) {
  TestShownTimeRecorded();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class FooterControllerEnterpriseTest
    : public FooterControllerExtensionTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  FooterControllerEnterpriseTest() {
    feature_list_.InitWithFeatureStates(
        {{ntp_features::kNtpFooter, true},
         {features::kEnterpriseBadgingForNtpFooter, true},
         {features::kSideBySide, std::get<0>(GetParam())}});
  }
  ~FooterControllerEnterpriseTest() override = default;

  bool managed() { return std::get<1>(GetParam()); }
  PrefService* local_state() { return g_browser_process->local_state(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         FooterControllerEnterpriseTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(FooterControllerEnterpriseTest, NoticePolicyEnabled) {
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      managed() ? policy::EnterpriseManagementAuthority::DOMAIN_LOCAL
                : policy::EnterpriseManagementAuthority::NONE);

  // Non-NTP
  ASSERT_FALSE(footer()->GetVisible());

  // Default NTP
  NavigateCurrentTab(GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(managed(), footer()->GetVisible());

  // 1P NTP
  NavigateCurrentTab(GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_EQ(managed(), footer()->GetVisible());

  // 3P NTP
  NavigateCurrentTab(GURL(chrome::kChromeUINewTabPageThirdPartyURL));
  EXPECT_EQ(managed(), footer()->GetVisible());

  // Extension NTP
  auto extension = LoadNtpExtension();
  NavigateCurrentTab(extension->url());
  EXPECT_TRUE(footer()->GetVisible());

  // Non-NTP
  NavigateCurrentTab(GURL(kNonNtpUrl));
  EXPECT_FALSE(footer()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(FooterControllerEnterpriseTest, NoticePolicyDisabled) {
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      managed() ? policy::EnterpriseManagementAuthority::DOMAIN_LOCAL
                : policy::EnterpriseManagementAuthority::NONE);
  local_state()->SetBoolean(prefs::kNTPFooterManagementNoticeEnabled, false);

  NavigateCurrentTab(GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_FALSE(footer()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(FooterControllerEnterpriseTest, NoticePolicyChanged) {
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      managed() ? policy::EnterpriseManagementAuthority::DOMAIN_LOCAL
                : policy::EnterpriseManagementAuthority::NONE);
  ASSERT_FALSE(footer()->GetVisible());

  NavigateCurrentTab(GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(managed(), footer()->GetVisible());

  local_state()->SetBoolean(prefs::kNTPFooterManagementNoticeEnabled, false);
  EXPECT_FALSE(footer()->GetVisible());

  local_state()->SetBoolean(prefs::kNTPFooterManagementNoticeEnabled, true);
  EXPECT_EQ(managed(), footer()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(FooterControllerEnterpriseTest,
                       NoticeItemMetricsRecorded) {
  if (!managed()) {
    GTEST_SKIP() << "This test is relevant only for managed case. Unmanaged "
                    "case is covered by the extension test.";
  }

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
  VerifyNoticeMetricsRecorded(0);

  NavigateCurrentTab(GURL(chrome::kChromeUINewTabURL));
  ASSERT_EQ(managed(), footer()->GetVisible());
  VerifyNoticeMetricsRecorded(/*total_count= */ 1, /*management_count= */ 1);

  auto extension = LoadNtpExtension();
  NavigateCurrentTab(extension->url());
  VerifyNoticeMetricsRecorded(/*total_count= */ 3, /*management_count= */ 2,
                              /*extension_count= */ 1);

  local_state()->SetBoolean(prefs::kNTPFooterManagementNoticeEnabled, false);
  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, false);
  NavigateCurrentTab(extension->url());
  VerifyNoticeMetricsRecorded(/*total_count= */ 3, /*management_count= */ 2,
                              /*extension_count= */ 1);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

class FooterControllerSideBySideTest
    : public FooterControllerExtensionTestBase {
 public:
  FooterControllerSideBySideTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter, features::kSideBySide},
        /*disabled_features=*/{});
  }
  ~FooterControllerSideBySideTest() override = default;

  void SetUpOnMainThread() override {
    FooterControllerExtensionTestBase::SetUpOnMainThread();
    NavigateCurrentTab(GURL(kNonNtpUrl));
    OpenNewTab(GURL(kNonNtpUrl));
    tab_strip_model()->AddToNewSplit(
        {0}, split_tabs::SplitTabVisualData(),
        split_tabs::SplitTabCreatedSource::kToolbarButton);
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }
};

class FooterControllerSideBySideSingleTabTest
    : public FooterControllerSideBySideTest,
      public testing::WithParamInterface<size_t> {
  void SetUpOnMainThread() override {
    FooterControllerSideBySideTest::SetUpOnMainThread();
    tab_strip_model()->ActivateTabAt(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         FooterControllerSideBySideSingleTabTest,
                         testing::Values(0, 1));

IN_PROC_BROWSER_TEST_P(FooterControllerSideBySideSingleTabTest, TabChanged) {
  auto extension = LoadNtpExtension();

  ASSERT_FALSE(footer()->GetVisible());

  NavigateCurrentTab(GURL(extension->url()));
  EXPECT_TRUE(footer()->GetVisible());

  NavigateCurrentTab(GURL(kNonNtpUrl));
  EXPECT_FALSE(footer()->GetVisible());

  NavigateCurrentTab(GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_FALSE(footer()->GetVisible());

  OpenNewTab(GURL(extension->url()));
  EXPECT_TRUE(footer()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(FooterControllerSideBySideSingleTabTest,
                       UserPrefChanged) {
  TestUserPrefChanged();
}

IN_PROC_BROWSER_TEST_P(FooterControllerSideBySideSingleTabTest,
                       AttributionPolicyChanged) {
  TestAttributionPolicyChanged();
}

IN_PROC_BROWSER_TEST_P(FooterControllerSideBySideSingleTabTest,
                       MetricsRecorded) {
  TestMetricsRecorded();
}

IN_PROC_BROWSER_TEST_P(FooterControllerSideBySideSingleTabTest,
                       ShownTimeRecorded) {
  TestShownTimeRecorded();
}

IN_PROC_BROWSER_TEST_F(FooterControllerSideBySideTest, SwapTabInSplit) {
  auto extension = LoadNtpExtension();

  // Create a non-split tab.
  OpenNewTab(GURL(extension->url()));
  ASSERT_TRUE(footer()->GetVisible());
  const int non_split_tab_index = tab_strip_model()->active_index();

  for (size_t index : {0, 1}) {
    tab_strip_model()->ActivateTabAt(index);

    ASSERT_FALSE(footer()->GetVisible());

    tab_strip_model()->UpdateTabInSplit(tab_strip_model()->GetActiveTab(),
                                        non_split_tab_index,
                                        TabStripModel::SplitUpdateType::kSwap);
    EXPECT_TRUE(footer()->GetVisible());

    tab_strip_model()->UpdateTabInSplit(tab_strip_model()->GetActiveTab(),
                                        non_split_tab_index,
                                        TabStripModel::SplitUpdateType::kSwap);
    EXPECT_FALSE(footer()->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_F(FooterControllerSideBySideTest, ReverseSplit) {
  auto extension = LoadNtpExtension();

  tab_strip_model()->ActivateTabAt(0);
  ASSERT_FALSE(footer()->GetVisible());
  tab_strip_model()->ActivateTabAt(1);
  NavigateCurrentTab(GURL(extension->url()));
  ASSERT_TRUE(footer()->GetVisible());

  tab_strip_model()->ReverseTabsInSplit(
      tab_strip_model()->GetActiveTab()->GetSplit().value());
  tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(footer()->GetVisible());
  tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(footer()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(FooterControllerSideBySideTest, CloseLeftTabInSplit) {
  auto extension = LoadNtpExtension();

  tab_strip_model()->ActivateTabAt(0);
  ASSERT_FALSE(footer()->GetVisible());
  tab_strip_model()->ActivateTabAt(1);
  NavigateCurrentTab(GURL(extension->url()));
  ASSERT_TRUE(footer()->GetVisible());

  tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE |
             TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  EXPECT_TRUE(footer()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(FooterControllerSideBySideTest, CloseRightTabInSplit) {
  auto extension = LoadNtpExtension();

  tab_strip_model()->ActivateTabAt(0);
  ASSERT_FALSE(footer()->GetVisible());
  tab_strip_model()->ActivateTabAt(1);
  NavigateCurrentTab(GURL(extension->url()));
  ASSERT_TRUE(footer()->GetVisible());

  tab_strip_model()->CloseWebContentsAt(
      1, TabCloseTypes::CLOSE_USER_GESTURE |
             TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  EXPECT_FALSE(footer()->GetVisible());
}
