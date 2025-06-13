// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/common/pref_names.h"
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

  new_tab_footer::NewTabFooterWebView* footer() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->new_tab_footer_web_view();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class FooterControllerExtensionTest : public FooterControllerExtensionTestBase {
 public:
  FooterControllerExtensionTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter},
        /*disabled_features=*/{features::kSideBySide});
  }
  ~FooterControllerExtensionTest() override = default;
};

IN_PROC_BROWSER_TEST_F(FooterControllerExtensionTest, TabChanged) {
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

IN_PROC_BROWSER_TEST_F(FooterControllerExtensionTest, UserPrefChanged) {
  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
  auto extension = LoadNtpExtension();
  NavigateCurrentTab(extension->url());
  ASSERT_FALSE(footer()->GetVisible());

  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
  EXPECT_TRUE(footer()->GetVisible());

  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
  EXPECT_FALSE(footer()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(FooterControllerExtensionTest,
                       AttributionPolicyChanged) {
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

IN_PROC_BROWSER_TEST_F(FooterControllerExtensionTest, VisibilityRecorded) {
  base::HistogramTester histogram_tester;
  const std::string& visible_on_load = "NewTabPage.Footer.VisibleOnLoad";

  auto extension = LoadNtpExtension();
  histogram_tester.ExpectTotalCount(visible_on_load, 0);

  NavigateCurrentTab(extension->url());
  histogram_tester.ExpectTotalCount(visible_on_load, 1);
  histogram_tester.ExpectBucketCount(visible_on_load, true, 1);

  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, false);
  histogram_tester.ExpectTotalCount(visible_on_load, 1);
  histogram_tester.ExpectBucketCount(visible_on_load, true, 1);

  NavigateCurrentTab(extension->url());
  histogram_tester.ExpectTotalCount(visible_on_load, 2);
  histogram_tester.ExpectBucketCount(visible_on_load, true, 1);
  histogram_tester.ExpectBucketCount(visible_on_load, false, 1);
}

IN_PROC_BROWSER_TEST_F(FooterControllerExtensionTest, ShownTimeRecorded) {
  base::HistogramTester histogram_tester;
  const std::string& shown_time = "NewTabPage.Footer.ShownTime";

  auto extension = LoadNtpExtension();
  histogram_tester.ExpectTotalCount(shown_time, 0);

  base::TimeTicks start = base::TimeTicks::Now();
  NavigateCurrentTab(extension->url());
  int max_expected = (base::TimeTicks::Now() - start).InMilliseconds();

  histogram_tester.ExpectTotalCount(shown_time, 1);
  int actual = histogram_tester.GetAllSamples(shown_time)[0].min;
  EXPECT_GT(actual, 1);
  EXPECT_LE(actual, max_expected);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class FooterControllerEnterpriseTest
    : public FooterControllerExtensionTestBase,
      public testing::WithParamInterface<bool> {
 public:
  FooterControllerEnterpriseTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter,
                              features::kEnterpriseBadgingForNtpFooter},
        /*disabled_features=*/{features::kSideBySide});
  }
  ~FooterControllerEnterpriseTest() override = default;

  bool managed() { return GetParam(); }
  PrefService* local_state() { return g_browser_process->local_state(); }
};

INSTANTIATE_TEST_SUITE_P(, FooterControllerEnterpriseTest, testing::Bool());

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
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

// TODO(crbug.com/4438803): Once the controller supports SideBySide enablement,
// refactor `FooterControllerExtensionTest` into a value-parameterized test,
// making `FooterControllerSideBySideTest` and
// `FooterControllerExtensionTestBase` redundant.
class FooterControllerSideBySideTest : public InProcessBrowserTest {
 public:
  FooterControllerSideBySideTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter, features::kSideBySide},
        /*disabled_features=*/{});
  }
  ~FooterControllerSideBySideTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FooterControllerSideBySideTest, FooterNotCreated) {
  auto* footer = BrowserView::GetBrowserViewForBrowser(browser())
                     ->new_tab_footer_web_view();
  EXPECT_FALSE(footer);
}
