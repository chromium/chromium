// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/safe_browsing_triggered_popup_blocker.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "components/safe_browsing/db/v4_embedded_test_server_util.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using safe_browsing::SubresourceFilterType;
using safe_browsing::SubresourceFilterLevel;

namespace {

base::LazyInstance<std::vector<std::string>>::Leaky g_error_messages_ =
    LAZY_INSTANCE_INITIALIZER;

class ScopedLoggingObserver {
 public:
  ScopedLoggingObserver()
      : old_log_handler_(logging::GetLogMessageHandler()),
        error_index_(g_error_messages_.Get().size()) {
    logging::SetLogMessageHandler(&ScopedLoggingObserver::LogHandler);
  }
  ~ScopedLoggingObserver() { logging::SetLogMessageHandler(old_log_handler_); }

  void RoundTripAndVerifyLogMessages(
      content::WebContents* web_contents,
      std::vector<std::string> messages_expected,
      std::vector<std::string> messages_not_expected) {
    // Round trip to the renderer to ensure the message would have gotten sent.
    EXPECT_TRUE(content::ExecuteScript(web_contents, "var a = 1;"));
    std::deque<bool> verify_messages_expected(messages_expected.size(), false);
    for (size_t i = 0; i < messages_expected.size(); i++)
      verify_messages_expected[i] = false;

    for (size_t i = error_index_; i < g_error_messages_.Get().size(); ++i) {
      const std::string& message = g_error_messages_.Get()[i];
      for (size_t i = 0; i < messages_expected.size(); i++) {
        verify_messages_expected[i] |=
            (message.find(messages_expected[i]) != std::string::npos);
      }
      for (const auto& message_not_expected : messages_not_expected)
        EXPECT_EQ(std::string::npos, message.find(message_not_expected))
            << message_not_expected;
    }
    for (size_t i = 0; i < messages_expected.size(); i++)
      EXPECT_TRUE(verify_messages_expected[i])
          << "Expected: " << messages_expected[i];
  }

 private:
  // Intercepts all console messages. Only used when the ConsoleObserverDelegate
  // cannot be (e.g. when we need the standard delegate).
  static bool LogHandler(int severity,
                         const char* file,
                         int line,
                         size_t message_start,
                         const std::string& str) {
    if (file && std::string("CONSOLE") == file)
      g_error_messages_.Get().push_back(str);
    return false;
  }

  logging::LogMessageHandlerFunction old_log_handler_;

  // The index into |g_error_messages| when this object was instantiated. Used
  // for iterating through messages received since this object was created.
  size_t error_index_;
  DISALLOW_COPY_AND_ASSIGN(ScopedLoggingObserver);
};

}  // namespace

// Tests for the subresource_filter popup blocker.
class SafeBrowsingTriggeredPopupBlockerBrowserTest
    : public InProcessBrowserTest {
 public:
  SafeBrowsingTriggeredPopupBlockerBrowserTest() {
    // Note the safe browsing popup blocker is still reliant on
    // SubresourceFilter to get notifications from the safe browsing navigation
    // throttle. We could consider separating that out in the future.
    scoped_feature_list_.InitWithFeatures(
        {subresource_filter::kSafeBrowsingSubresourceFilter,
         kAbusiveExperienceEnforce},
        {});
  }

  ~SafeBrowsingTriggeredPopupBlockerBrowserTest() override {}

  void SetUp() override {
    FinalizeFeatures();
    database_helper_ = CreateTestDatabase();
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());
  }
  void TearDown() override {
    InProcessBrowserTest::TearDown();
    database_helper_.reset();
  }

  virtual void FinalizeFeatures() {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  virtual std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase() {
    std::vector<safe_browsing::ListIdentifier> list_ids = {
        safe_browsing::GetUrlSubresourceFilterId()};
    return std::make_unique<TestSafeBrowsingDatabaseHelper>(
        std::make_unique<safe_browsing::TestV4GetHashProtocolManagerFactory>(),
        std::move(list_ids));
  }

  void ConfigureAsAbusive(const GURL& url) {
    safe_browsing::ThreatMetadata metadata;
    metadata.subresource_filter_match = {
        {SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::ENFORCE}};
    database_helper_->AddFullHashToDbAndFullHashCache(
        url, safe_browsing::GetUrlSubresourceFilterId(), metadata);
  }
  void ConfigureAsAbusiveWarn(const GURL& url) {
    safe_browsing::ThreatMetadata metadata;
    metadata.subresource_filter_match = {
        {SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::WARN}};
    database_helper()->AddFullHashToDbAndFullHashCache(
        url, safe_browsing::GetUrlSubresourceFilterId(), metadata);
  }

  TestSafeBrowsingDatabaseHelper* database_helper() {
    return database_helper_.get();
  }

  void UpdatePolicy(const policy::PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestSafeBrowsingDatabaseHelper> database_helper_;
  std::unique_ptr<SafeBrowsingTriggeredPopupBlocker> popup_blocker_;
  policy::MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTriggeredPopupBlockerBrowserTest);
};

class SafeBrowsingTriggeredPopupBlockerDisabledTest
    : public SafeBrowsingTriggeredPopupBlockerBrowserTest {
  void FinalizeFeatures() override {
    scoped_feature_list_.InitAndDisableFeature(kAbusiveExperienceEnforce);
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test harness does not mock the safe browsing v4 hash protocol manager.
// Instead, it mocks actual HTTP responses from the v4 server by redirecting
// requests to a custom test server with a special full hash request handler.
class SafeBrowsingTriggeredInterceptingBrowserTest
    : public SafeBrowsingTriggeredPopupBlockerBrowserTest {
 public:
  SafeBrowsingTriggeredInterceptingBrowserTest()
      : safe_browsing_server_(
            std::make_unique<net::test_server::EmbeddedTestServer>()) {}
  ~SafeBrowsingTriggeredInterceptingBrowserTest() override {}

  // SafeBrowsingTriggeredPopupBlockerBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(safe_browsing_server_->InitializeAndListen());
    SafeBrowsingTriggeredPopupBlockerBrowserTest::SetUp();
  }
  std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase()
      override {
    std::vector<safe_browsing::ListIdentifier> list_ids = {
        safe_browsing::GetUrlSubresourceFilterId()};
    // Send a nullptr TestV4GetHashProtocolManager so full hash requests aren't
    // mocked.
    return std::make_unique<TestSafeBrowsingDatabaseHelper>(
        nullptr, std::move(list_ids));
  }

  net::test_server::EmbeddedTestServer* safe_browsing_server() {
    return safe_browsing_server_.get();
  }

  safe_browsing::ThreatMatch GetAbusiveMatch(const GURL& url,
                                             const std::string& abusive_value) {
    safe_browsing::ThreatMatch threat_match;
    threat_match.set_threat_type(safe_browsing::SUBRESOURCE_FILTER);
    threat_match.set_platform_type(
        safe_browsing::GetUrlSubresourceFilterId().platform_type());
    threat_match.set_threat_entry_type(safe_browsing::URL);

    safe_browsing::FullHash enforce_full_hash =
        safe_browsing::V4ProtocolManagerUtil::GetFullHash(url);
    threat_match.mutable_threat()->set_hash(enforce_full_hash);
    threat_match.mutable_cache_duration()->set_seconds(300);

    safe_browsing::ThreatEntryMetadata::MetadataEntry* threat_meta =
        threat_match.mutable_threat_entry_metadata()->add_entries();
    threat_meta->set_key("sf_absv");
    threat_meta->set_value(abusive_value);
    return threat_match;
  }

 private:
  std::unique_ptr<net::test_server::EmbeddedTestServer> safe_browsing_server_;
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTriggeredInterceptingBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerDisabledTest,
                       NoFeature_AllowCreatingNewWindows) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsAbusive(a_url);

  // Navigate to a_url, should not trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerDisabledTest,
                       NoFeature_NoMessages) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsAbusiveWarn(a_url);

  // Navigate to a_url, should not log any warning messages.
  ScopedLoggingObserver log_observer;
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(ContentSettingsType::POPUPS));

  log_observer.RoundTripAndVerifyLogMessages(
      web_contents, {}, {kAbusiveWarnMessage, kAbusiveEnforceMessage});
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       DrivenByEnterprisePolicy) {
  // Disable Abusive experience intervention policy.
  policy::PolicyMap policy;
  policy.Set(policy::key::kAbusiveExperienceInterventionEnforce,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             std::make_unique<base::Value>(false), nullptr);
  UpdatePolicy(policy);

  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsAbusive(a_url);

  // Navigate to a_url, should not trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Since the policy change can take effect without browser restart, verify
  // that enabling the policy here should disallow opening new tabs or windows
  // from any new tab opened after the policy is set.
  policy.Set(policy::key::kAbusiveExperienceInterventionEnforce,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             std::make_unique<base::Value>(true), nullptr);
  UpdatePolicy(policy);

  // Open a new tab to make sure the SafeBrowsingTriggeredPopupBlocker gets
  // created for the new tab.
  chrome::NewTab(browser());

  // Navigate to a_url, should trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  opened_window = false;
  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents1, "openWindow()", &opened_window));
  EXPECT_FALSE(opened_window);
  // Make sure the popup UI was shown.
  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents1)
                  ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       NoList_AllowCreatingNewWindows) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));

  // Mark as matching social engineering, not subresource filter.
  safe_browsing::ThreatMetadata metadata;
  database_helper()->AddFullHashToDbAndFullHashCache(
      a_url, safe_browsing::GetUrlSocEngId(), metadata);

  // Navigate to a_url, should not trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       NoAbusive_AllowCreatingNewWindows) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));

  // Navigate to a_url, should not trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       BlockCreatingNewWindows) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  GURL b_url(embedded_test_server()->GetURL("b.com", kWindowOpenPath));
  ConfigureAsAbusive(a_url);

  // Navigate to a_url, should trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_FALSE(opened_window);
  // Make sure the popup UI was shown.
  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Block again.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_FALSE(opened_window);

  // Navigate to |b_url|, which should successfully open the popup.
  ui_test_utils::NavigateToURL(browser(), b_url);
  opened_window = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
  // Popup UI should not be shown.
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       ShowBlockedPopup) {
  base::HistogramTester tester;

  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsAbusive(a_url);

  // Navigate to a_url, should trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_FALSE(opened_window);

  // Make sure the popup UI was shown.
  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Click through.
  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  auto* popup_blocker = PopupBlockerTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  popup_blocker->ShowBlockedPopup(
      popup_blocker->GetBlockedPopupRequests().begin()->first,
      WindowOpenDisposition::NEW_BACKGROUND_TAB);

  const char kPopupActions[] = "ContentSettings.Popups.BlockerActions";
  tester.ExpectBucketCount(
      kPopupActions,
      static_cast<int>(PopupBlockerTabHelper::Action::kClickedThroughAbusive),
      1);

  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       BlockCreatingNewWindows_LogsToConsole) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kAbusiveEnforceMessage);
  web_contents()->SetDelegate(&console_observer);
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsAbusive(a_url);

  // Navigate to a_url, should trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "openWindow()", &opened_window));
  EXPECT_FALSE(opened_window);
  console_observer.Wait();
  EXPECT_EQ(kAbusiveEnforceMessage, console_observer.message());
}

// Whitelisted sites should not have console logging.
IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       AllowCreatingNewWindows_NoLogToConsole) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsAbusive(a_url);

  // Allow popups on |a_url|.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      a_url, a_url, ContentSettingsType::POPUPS, std::string(),
      CONTENT_SETTING_ALLOW);

  // Navigate to a_url, should not trigger the popup blocker.
  ScopedLoggingObserver log_observer;
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "openWindow()", &opened_window));
  EXPECT_TRUE(opened_window);
  log_observer.RoundTripAndVerifyLogMessages(
      web_contents(), {}, {kAbusiveEnforceMessage, kAbusiveWarnMessage});
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       BlockOpenURLFromTab) {
  const char kWindowOpenPath[] =
      "/subresource_filter/window_open_spoof_click.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  GURL b_url(embedded_test_server()->GetURL("b.com", kWindowOpenPath));
  ConfigureAsAbusive(a_url);

  // Navigate to a_url, should trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(web_contents, "openWindow()"));

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Navigate to |b_url|, which should successfully open the popup.

  ui_test_utils::NavigateToURL(browser(), b_url);

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(web_contents, "openWindow()"));
  navigation_observer.Wait();

  // Popup UI should not be shown.
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       BlockOpenURLFromTabInIframe) {
  const char popup_path[] = "/subresource_filter/iframe_spoof_click_popup.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", popup_path));
  ConfigureAsAbusive(a_url);

  // Navigate to a_url, should not trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool sent_open = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &sent_open));
  EXPECT_TRUE(sent_open);
  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       MultipleNavigations) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  const GURL url1(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  const GURL url2(embedded_test_server()->GetURL("b.com", kWindowOpenPath));
  ConfigureAsAbusive(url1);

  auto open_popup_and_expect_block = [&](bool expect_block) {
    bool opened_window = false;
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents, "openWindow()", &opened_window));
    EXPECT_EQ(expect_block, !opened_window);
    EXPECT_EQ(expect_block,
              TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(ContentSettingsType::POPUPS));
  };

  ui_test_utils::NavigateToURL(browser(), url1);
  open_popup_and_expect_block(true);

  ui_test_utils::NavigateToURL(browser(), url2);
  open_popup_and_expect_block(false);

  ui_test_utils::NavigateToURL(browser(), url1);
  open_popup_and_expect_block(true);

  ui_test_utils::NavigateToURL(browser(), url2);
  open_popup_and_expect_block(false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       WarningDoNotBlockCreatingNewWindows_LogsToConsole) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));

  ConfigureAsAbusiveWarn(a_url);

  // Navigate to a_url, should log a warning and not trigger the popup blocker.
  ScopedLoggingObserver log_observer;

  ui_test_utils::NavigateToURL(browser(), a_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  log_observer.RoundTripAndVerifyLogMessages(
      web_contents, {kAbusiveWarnMessage}, {kAbusiveEnforceMessage});

  bool opened_window = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
}

// If the site activates in warning mode, make sure warning messages are logged
// even if the user has popups whitelisted via settings.
IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       WarningAllowCreatingNewWindows_LogsToConsole) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));

  ConfigureAsAbusiveWarn(a_url);

  // Allow popups on |a_url|.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      a_url, a_url, ContentSettingsType::POPUPS, std::string(),
      CONTENT_SETTING_ALLOW);

  // Navigate to a_url, should not trigger the popup blocker.
  ScopedLoggingObserver log_observer;

  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "openWindow()", &opened_window));
  EXPECT_TRUE(opened_window);

  log_observer.RoundTripAndVerifyLogMessages(
      web_contents(), {kAbusiveWarnMessage}, {kAbusiveEnforceMessage});
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredInterceptingBrowserTest,
                       AbusiveMetadata) {
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  const GURL no_match_url(
      embedded_test_server()->GetURL("no_match.com", kWindowOpenPath));
  const GURL enforce_url(
      embedded_test_server()->GetURL("enforce.com", kWindowOpenPath));
  const GURL warn_url(
      embedded_test_server()->GetURL("warn.com", kWindowOpenPath));

  // Mark the prefixes as bad so that safe browsing will request full hashes
  // from the v4 server. Even mark the no_match URL as bad just to test that the
  // custom server handler is working properly.
  database_helper()->LocallyMarkPrefixAsBad(
      no_match_url, safe_browsing::GetUrlSubresourceFilterId());
  database_helper()->LocallyMarkPrefixAsBad(
      warn_url, safe_browsing::GetUrlSubresourceFilterId());
  database_helper()->LocallyMarkPrefixAsBad(
      enforce_url, safe_browsing::GetUrlSubresourceFilterId());

  // Register the V4 server to handle full hash requests for the two URLs, with
  // the given ThreatMatches, then start accepting connections on the v4 server.
  // Then, start the server.
  std::map<GURL, safe_browsing::ThreatMatch> response_map{
      {enforce_url, GetAbusiveMatch(enforce_url, "enforce")},
      {warn_url, GetAbusiveMatch(warn_url, "warn")}};
  safe_browsing::StartRedirectingV4RequestsForTesting(response_map,
                                                      safe_browsing_server());
  safe_browsing_server()->StartAcceptingConnections();

  // URL with no match should not trigger the blocker.
  {
    ScopedLoggingObserver log_observer;
    ui_test_utils::NavigateToURL(browser(), no_match_url);
    bool opened_window = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents(), "openWindow()", &opened_window));
    EXPECT_TRUE(opened_window);
    log_observer.RoundTripAndVerifyLogMessages(
        web_contents(), {}, {kAbusiveEnforceMessage, kAbusiveWarnMessage});
  }
  {
    ScopedLoggingObserver log_observer;
    ui_test_utils::NavigateToURL(browser(), enforce_url);
    bool opened_window = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents(), "openWindow()", &opened_window));
    EXPECT_FALSE(opened_window);
    log_observer.RoundTripAndVerifyLogMessages(
        web_contents(), {kAbusiveEnforceMessage}, {kAbusiveWarnMessage});
  }
  {
    ScopedLoggingObserver log_observer;
    ui_test_utils::NavigateToURL(browser(), warn_url);
    bool opened_window = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents(), "openWindow()", &opened_window));
    EXPECT_TRUE(opened_window);
    log_observer.RoundTripAndVerifyLogMessages(
        web_contents(), {kAbusiveWarnMessage}, {kAbusiveEnforceMessage});
  }
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingTriggeredPopupBlockerBrowserTest,
                       AbusivePagesAreNotPutIntoBackForwardCache) {
  content::BackForwardCacheDisabledTester back_forward_cache_tester;
  const GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ConfigureAsAbusive(a_url);

  // Navigate to an abusive page.
  ui_test_utils::NavigateToURL(browser(), a_url);

  content::RenderFrameHost* main_frame =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int main_frame_routing_id = main_frame->GetRoutingID();

  // Navigate away from the abusive page. This should block bfcache.
  ui_test_utils::NavigateToURL(browser(), b_url);

  EXPECT_TRUE(back_forward_cache_tester.IsDisabledForFrameWithReason(
      main_frame_process_id, main_frame_routing_id,
      "SafeBrowsingTriggeredPopupBlocker"));
}
