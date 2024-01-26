// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "chrome/renderer/net/available_offline_content_helper.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/net_error_info.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/net_errors.h"
#include "net/dns/public/resolve_error_info.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#endif

namespace {

const char kFailedUrl[] = "http://failed/";

// Creates a string from an error that is used as a mock locally generated
// error page for that error.
std::string ErrorToString(const error_page::Error& error, bool is_failed_post) {
  std::ostringstream ss;
  ss << "(" << error.url() << ", " << error.domain() << ", " << error.reason()
     << ", " << (is_failed_post ? "POST" : "NOT POST") << ")";
  return ss.str();
}

error_page::Error ProbeError(error_page::DnsProbeStatus status) {
  return error_page::Error::DnsProbeError(GURL(kFailedUrl), status, false);
}

error_page::Error NetErrorForURL(net::Error net_error, const GURL& url) {
  return error_page::Error::NetError(url, net_error, 0 /* extended_reason */,
                                     net::ResolveErrorInfo(net::OK), false);
}

error_page::Error NetError(net::Error net_error) {
  return error_page::Error::NetError(GURL(kFailedUrl), net_error,
                                     0 /* extended_reason */,
                                     net::ResolveErrorInfo(net::OK), false);
}

// Convenience functions that create an error string for a non-POST request.

std::string ProbeErrorString(error_page::DnsProbeStatus status) {
  return ErrorToString(ProbeError(status), false);
}

std::string NetErrorStringForURL(net::Error net_error, const GURL& url) {
  return ErrorToString(NetErrorForURL(net_error, url), false);
}

std::string NetErrorString(net::Error net_error) {
  return ErrorToString(NetError(net_error), false);
}

error_page::LocalizedError::PageState GetErrorPageState(int error_code,
                                                        bool is_kiosk_mode) {
  return error_page::LocalizedError::GetPageState(
      error_code, error_page::Error::kNetErrorDomain, GURL(kFailedUrl),
      /*is_post=*/false,
      /*is_secure_dns_network_error=*/false, /*stale_copy_in_cache=*/false,
      /*can_show_network_diagnostics_dialog=*/false, /*is_incognito=*/false,
      /*offline_content_feature_enabled=*/false,
      /*auto_fetch_feature_enabled=*/false, /*is_kiosk_mode=*/is_kiosk_mode,
      /*locale=*/"",
      /*is_blocked_by_extension=*/false,
      /*error_page_params=*/nullptr);
}

class NetErrorHelperCoreTest : public testing::Test,
                               public NetErrorHelperCore::Delegate {
 public:
  NetErrorHelperCoreTest()
      : update_count_(0),
        reload_count_(0),
        diagnose_error_count_(0),
        download_count_(0),
        list_visible_by_prefs_(true),
        enable_page_helper_functions_count_(0),
        default_url_(GURL(kFailedUrl)),
        error_url_(GURL(content::kUnreachableWebDataURL)) {}

  ~NetErrorHelperCoreTest() override = default;

  NetErrorHelperCore* core() { return &core_; }

  int reload_count() const { return reload_count_; }

  int diagnose_error_count() const { return diagnose_error_count_; }

  const GURL& diagnose_error_url() const { return diagnose_error_url_; }

  int download_count() const { return download_count_; }

  const GURL& default_url() const { return default_url_; }

  const GURL& error_url() const { return error_url_; }

  int enable_page_helper_functions_count() const {
    return enable_page_helper_functions_count_;
  }

  const std::string& last_update_string() const { return last_update_string_; }
  int update_count() const { return update_count_; }

  const std::string& last_error_html() const { return last_error_html_; }

  bool last_can_show_network_diagnostics_dialog() const {
    return last_can_show_network_diagnostics_dialog_;
  }

  void set_offline_content_feature_enabled(
      bool offline_content_feature_enabled) {
    offline_content_feature_enabled_ = offline_content_feature_enabled;
  }

  bool list_visible_by_prefs() const { return list_visible_by_prefs_; }

  void set_auto_fetch_allowed(bool allowed) { auto_fetch_allowed_ = allowed; }

  void set_is_offline_error(bool is_offline_error) {
    is_offline_error_ = is_offline_error;
  }

  const std::string& offline_content_json() const {
    return offline_content_json_;
  }

  const std::string& offline_content_summary_json() const {
    return offline_content_summary_json_;
  }

#if BUILDFLAG(IS_ANDROID)
  // State of auto fetch, as reported to Delegate. Unset if SetAutoFetchState
  // was not called.
  std::optional<chrome::mojom::OfflinePageAutoFetcherScheduleResult>
  auto_fetch_state() const {
    return auto_fetch_state_;
  }
#endif

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }
  content::MockRenderThread* render_thread() { return &render_thread_; }

  void DoErrorLoadOfURL(net::Error error, const GURL& url) {
    std::string html;
    core()->PrepareErrorPage(NetErrorHelperCore::MAIN_FRAME,
                             NetErrorForURL(error, url),
                             /*is_failed_post=*/false,
                             /*alternative_error_page_info=*/nullptr, &html);
    EXPECT_FALSE(html.empty());
    EXPECT_EQ(NetErrorStringForURL(error, url), html);

    core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
    core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  }

  void DoErrorLoad(net::Error error) {
    DoErrorLoadOfURL(error, GURL(kFailedUrl));
  }

  void DoSuccessLoad() {
    core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, default_url());
    core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  }

  void DoDnsProbe(error_page::DnsProbeStatus final_status) {
    core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
    core()->OnNetErrorInfo(final_status);
  }

 private:
  error_page::LocalizedError::PageState GetPageState() const {
    error_page::LocalizedError::PageState result;
    result.auto_fetch_allowed = auto_fetch_allowed_;
    result.offline_content_feature_enabled = offline_content_feature_enabled_;
    result.is_offline_error = is_offline_error_;
    return result;
  }

  // NetErrorHelperCore::Delegate implementation:
  error_page::LocalizedError::PageState GenerateLocalizedErrorPage(
      const error_page::Error& error,
      bool is_failed_post,
      bool can_show_network_diagnostics_dialog,
      content::mojom::AlternativeErrorPageOverrideInfoPtr
          alternative_error_page_info,
      std::string* html) override {
    last_can_show_network_diagnostics_dialog_ =
        can_show_network_diagnostics_dialog;

    CHECK_NE(html, nullptr);
    *html = ErrorToString(error, is_failed_post);

    return GetPageState();
  }

  void EnablePageHelperFunctions() override {
    enable_page_helper_functions_count_++;
  }

  error_page::LocalizedError::PageState UpdateErrorPage(
      const error_page::Error& error,
      bool is_failed_post,
      bool can_show_network_diagnostics_dialog) override {
    update_count_++;
    last_can_show_network_diagnostics_dialog_ =
        can_show_network_diagnostics_dialog;
    last_error_html_ = ErrorToString(error, is_failed_post);

    return GetPageState();
  }

  void InitializeErrorPageEasterEggHighScore(int high_score) override {}
  void RequestEasterEggHighScore() override {}

  void ReloadFrame() override { reload_count_++; }

  void DiagnoseError(const GURL& page_url) override {
    diagnose_error_count_++;
    diagnose_error_url_ = page_url;
  }

  void PortalSignin() override {}

  void DownloadPageLater() override { download_count_++; }

  void SetIsShowingDownloadButton(bool show) override {}

  void OfflineContentAvailable(
      bool list_visible_by_prefs,
      const std::string& offline_content_json) override {
    list_visible_by_prefs_ = list_visible_by_prefs;
    offline_content_json_ = offline_content_json;
  }

#if BUILDFLAG(IS_ANDROID)
  void SetAutoFetchState(
      chrome::mojom::OfflinePageAutoFetcherScheduleResult result) override {
    auto_fetch_state_ = result;
  }
#endif

  content::RenderFrame* GetRenderFrame() override { return nullptr; }

  base::test::TaskEnvironment task_environment_;
  content::MockRenderThread render_thread_;

  NetErrorHelperCore core_{this};

  // Contains the information passed to the last call to UpdateErrorPage, as a
  // string.
  std::string last_update_string_;
  // Number of times |last_update_string_| has been changed.
  int update_count_;

  // Contains the HTML set by the last call to UpdateErrorPage.
  std::string last_error_html_;

  // Values passed in to the last call of GenerateLocalizedErrorPage or
  // UpdateErrorPage.  Mutable because GenerateLocalizedErrorPage is const.
  mutable bool last_can_show_network_diagnostics_dialog_;

  int reload_count_;
  int diagnose_error_count_;
  GURL diagnose_error_url_;
  int download_count_;
  bool list_visible_by_prefs_;
  std::string offline_content_json_;
  std::string offline_content_summary_json_;
#if BUILDFLAG(IS_ANDROID)
  std::optional<chrome::mojom::OfflinePageAutoFetcherScheduleResult>
      auto_fetch_state_;
#endif
  bool offline_content_feature_enabled_ = false;
  bool is_offline_error_ = false;
  bool auto_fetch_allowed_ = false;

  int enable_page_helper_functions_count_;

  const GURL default_url_;
  const GURL error_url_;
};

//------------------------------------------------------------------------------
// Basic tests that don't update the error page for probes.
//------------------------------------------------------------------------------

TEST_F(NetErrorHelperCoreTest, Null) {}

TEST_F(NetErrorHelperCoreTest, SuccessfulPageLoad) {
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, default_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
}

TEST_F(NetErrorHelperCoreTest, MainFrameNonDnsError) {
  // An error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_CONNECTION_RESET),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page.
  EXPECT_FALSE(html.empty());
  EXPECT_EQ(NetErrorString(net::ERR_CONNECTION_RESET), html);

  // Error page loads.
  EXPECT_EQ(0, enable_page_helper_functions_count());
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(1, enable_page_helper_functions_count());
}

// Much like above tests, but with a bunch of spurious DNS status messages that
// should have no effect.
TEST_F(NetErrorHelperCoreTest, MainFrameNonDnsErrorSpuriousStatus) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_CONNECTION_RESET),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  // Should have returned a local error page.
  EXPECT_FALSE(html.empty());
  EXPECT_EQ(NetErrorString(net::ERR_CONNECTION_RESET), html);

  // Error page loads.

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  EXPECT_EQ(0, update_count());
}

TEST_F(NetErrorHelperCoreTest,
       UserModeErrBlockedByAdministratorContainsDetails) {
  error_page::LocalizedError::PageState page_state = GetErrorPageState(
      net::ERR_BLOCKED_BY_ADMINISTRATOR, /*is_kiosk_mode=*/false);

  auto* suggestions_details = page_state.strings.FindList("suggestionsDetails");
  ASSERT_TRUE(suggestions_details);
  ASSERT_TRUE(suggestions_details->empty());

  auto* suggestions_summary_list =
      page_state.strings.FindList("suggestionsSummaryList");
  ASSERT_TRUE(suggestions_summary_list);
  EXPECT_TRUE(suggestions_summary_list->empty());
}

TEST_F(NetErrorHelperCoreTest,
       KioskModeErrBlockedByAdministratorDoenNotContainDetails) {
  error_page::LocalizedError::PageState page_state = GetErrorPageState(
      net::ERR_BLOCKED_BY_ADMINISTRATOR, /*is_kiosk_mode=*/true);

  auto* suggestions_details = page_state.strings.FindList("suggestionsDetails");
  ASSERT_TRUE(suggestions_details);
  EXPECT_TRUE(suggestions_details->empty());

  auto* suggestions_summary_list =
      page_state.strings.FindList("suggestionsSummaryList");
  ASSERT_TRUE(suggestions_summary_list);
  EXPECT_TRUE(suggestions_summary_list->empty());
}

TEST_F(NetErrorHelperCoreTest, SubFrameErrorWithCustomErrorPage) {
  // Loading fails, and an error page is requested. |error_html| is null
  // indicating a custom error page. Calls below should not crash.
  core()->PrepareErrorPage(
      NetErrorHelperCore::SUB_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr,
      /*error_html=*/nullptr);
  core()->OnCommitLoad(NetErrorHelperCore::SUB_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  EXPECT_EQ(0, update_count());
}

TEST_F(NetErrorHelperCoreTest, SubFrameDnsError) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::SUB_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page.
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::SUB_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  EXPECT_EQ(0, update_count());
}

// Much like above tests, but with a bunch of spurious DNS status messages that
// should have no effect.
TEST_F(NetErrorHelperCoreTest, SubFrameDnsErrorSpuriousStatus) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  core()->PrepareErrorPage(
      NetErrorHelperCore::SUB_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  // Should have returned a local error page.
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), html);

  // Error page loads.

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  core()->OnCommitLoad(NetErrorHelperCore::SUB_FRAME, error_url());
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  core()->OnFinishLoad(NetErrorHelperCore::SUB_FRAME);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);

  EXPECT_EQ(0, update_count());
}

//------------------------------------------------------------------------------
// Tests for updating the error page in response to DNS probe results.
//------------------------------------------------------------------------------

// Test case where the error page finishes loading before receiving any DNS
// probe messages.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbe) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
}

// Same as above, but the probe is not run.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeNotRun) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  // When the not run status arrives, the page should revert to the normal dns
  // error page.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_NOT_RUN);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(1, update_count());
}

// Same as above, but the probe result is inconclusive.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeInconclusive) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // When the inconclusive status arrives, the page should revert to the normal
  // dns error page.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_INCONCLUSIVE);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(NetErrorString(net::ERR_NAME_NOT_RESOLVED), last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_INCONCLUSIVE);
  EXPECT_EQ(2, update_count());
}

// Same as above, but the probe result is no internet.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeNoInternet) {
  base::HistogramTester histogram_tester_;

  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // The final status arrives, and should display the offline error page.
  set_is_offline_error(true);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts", error_page::NETWORK_ERROR_PAGE_OFFLINE_ERROR_SHOWN,
      1);

  // Any other probe updates should be ignored.
  set_is_offline_error(false);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());

  // Perform a second error page load, and confirm that the previous load
  // doesn't affect the result.
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());
  set_is_offline_error(true);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts", error_page::NETWORK_ERROR_PAGE_OFFLINE_ERROR_SHOWN,
      2);
}

// Same as above, but the probe result is bad config.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbeBadConfig) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // When the inconclusive status arrives, the page should revert to the normal
  // dns error page.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_BAD_CONFIG),
            last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
  EXPECT_EQ(2, update_count());
}

// Test case where the error page finishes loading after receiving the start
// DNS probe message.
TEST_F(NetErrorHelperCoreTest, FinishedAfterStartProbe) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());

  // Nothing should be done when a probe status comes in before loading
  // finishes.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(0, update_count());

  // When loading finishes, however, the buffered probe status should be sent
  // to the page.
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // Should update the page again when the probe result comes in.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_NOT_RUN);
  EXPECT_EQ(2, update_count());
}

// Test case where the error page finishes loading before receiving any DNS
// probe messages and the request is a POST.
TEST_F(NetErrorHelperCoreTest, FinishedBeforeProbePost) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/true, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ErrorToString(ProbeError(error_page::DNS_PROBE_POSSIBLE), true),
            html);

  // Error page loads.
  EXPECT_EQ(0, enable_page_helper_functions_count());
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(0, update_count());
  EXPECT_EQ(1, enable_page_helper_functions_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ErrorToString(ProbeError(error_page::DNS_PROBE_STARTED), true),
            last_error_html());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(
      ErrorToString(ProbeError(error_page::DNS_PROBE_FINISHED_NXDOMAIN), true),
      last_error_html());
}

// Test case where the probe finishes before the page is committed.
TEST_F(NetErrorHelperCoreTest, ProbeFinishesEarly) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page starts loading. Nothing should be done when the probe statuses
  // come in before loading finishes.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  EXPECT_EQ(0, update_count());

  // When loading finishes, however, the buffered probe status should be sent
  // to the page.
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Any other probe updates should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(1, update_count());
}

// Test case where one error page loads completely before a new navigation
// results in another error page.  Probes are run for both pages.
TEST_F(NetErrorHelperCoreTest, TwoErrorsWithProbes) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe results come in.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // The process starts again.

  // Loading fails, and an error page is requested.
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(2, update_count());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(3, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  // The probe returns a different result this time.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(4, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
}

// Test case where one error page loads completely before a new navigation
// results in another error page.  Probe results for the first probe are only
// received after the second load starts, but before it commits.
TEST_F(NetErrorHelperCoreTest, TwoErrorsWithProbesAfterSecondStarts) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page loads.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // The process starts again.

  // Loading fails, and an error page is requested.
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page starts to load. Probe results come in, and the first page is
  // updated.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Second page finishes loading, and is updated using the same probe result.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  EXPECT_EQ(3, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NXDOMAIN),
            last_error_html());

  // Other probe results should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(3, update_count());
}

// Same as above, but a new page is loaded before the error page commits.
TEST_F(NetErrorHelperCoreTest, ErrorPageLoadInterrupted) {
  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page starts loading. Probe statuses come in, but should be ignored.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());

  // A new navigation fails while the error page is loading.
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, /*alternative_error_page_info=*/nullptr, &html);
  // Should have returned a local error page indicating a probe may run.
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_POSSIBLE), html);

  // Error page finishes loading.
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);

  // Probe results come in.
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_STARTED), last_error_html());

  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_EQ(2, update_count());
  EXPECT_EQ(ProbeErrorString(error_page::DNS_PROBE_FINISHED_NO_INTERNET),
            last_error_html());
}

TEST_F(NetErrorHelperCoreTest, ExplicitReloadSucceeds) {
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(0, reload_count());
  core()->ExecuteButtonPress(NetErrorHelperCore::RELOAD_BUTTON);
  EXPECT_EQ(1, reload_count());
}

TEST_F(NetErrorHelperCoreTest, CanNotShowNetworkDiagnostics) {
  core()->OnSetCanShowNetworkDiagnosticsDialog(false);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_FALSE(last_can_show_network_diagnostics_dialog());
}

TEST_F(NetErrorHelperCoreTest, CanShowNetworkDiagnostics) {
  core()->OnSetCanShowNetworkDiagnosticsDialog(true);
  DoErrorLoad(net::ERR_CONNECTION_RESET);
  EXPECT_TRUE(last_can_show_network_diagnostics_dialog());

  core()->ExecuteButtonPress(NetErrorHelperCore::DIAGNOSE_ERROR);
  EXPECT_EQ(1, diagnose_error_count());
  EXPECT_EQ(GURL(kFailedUrl), diagnose_error_url());
}

TEST_F(NetErrorHelperCoreTest, AlternativeErrorPageNoUpdates) {
  // Relevant strings for the alternative error page can be found in
  // `chrome/browser/web_applications/web_app_offline.h`
  auto alternative_error_page_info =
      content::mojom::AlternativeErrorPageOverrideInfo::New();
  base::Value::Dict dict;
  dict.Set("theme_color", skia::SkColorToHexString(SK_ColorBLUE));
  dict.Set("customized_background_color",
           skia::SkColorToHexString(SK_ColorYELLOW));
  dict.Set("app_short_name", "Test Short Name");
  dict.Set(
      "web_app_error_page_message",
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED));
  alternative_error_page_info->alternative_error_page_params = std::move(dict);
  alternative_error_page_info->resource_id = IDR_WEBAPP_ERROR_PAGE_HTML;

  // Loading fails, and an error page is requested.
  std::string html;
  core()->PrepareErrorPage(
      NetErrorHelperCore::MAIN_FRAME, NetError(net::ERR_NAME_NOT_RESOLVED),
      /*is_failed_post=*/false, std::move(alternative_error_page_info), &html);

  // Expect that for all probe updates the error page does not change
  core()->OnCommitLoad(NetErrorHelperCore::MAIN_FRAME, error_url());
  core()->OnFinishLoad(NetErrorHelperCore::MAIN_FRAME);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_STARTED);
  core()->OnNetErrorInfo(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_EQ(0, update_count());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(NetErrorHelperCoreTest, Download) {
  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(0, download_count());
  core()->ExecuteButtonPress(NetErrorHelperCore::DOWNLOAD_BUTTON);
  EXPECT_EQ(1, download_count());
}

const char kThumbnailDataURI[] = "data:image/png;base64,abc";
const char kFaviconDataURI[] = "data:image/png;base64,def";

// Creates a couple of fake AvailableOfflineContent instances.
std::vector<chrome::mojom::AvailableOfflineContentPtr>
GetFakeAvailableContent() {
  std::vector<chrome::mojom::AvailableOfflineContentPtr> content;
  content.push_back(chrome::mojom::AvailableOfflineContent::New(
      "ID", "name_space", "title", "snippet", "date_modified", "attribution",
      GURL(kThumbnailDataURI), GURL(kFaviconDataURI),
      chrome::mojom::AvailableContentType::kPrefetchedPage));
  content.push_back(chrome::mojom::AvailableOfflineContent::New(
      "ID2", "name_space2", "title2", "snippet2", "date_modified2",
      "attribution2", GURL(kThumbnailDataURI), GURL(kFaviconDataURI),
      chrome::mojom::AvailableContentType::kOtherPage));
  return content;
}

// Builds the expected JSON representation of the AvailableOfflineContent
// instances returned by |GetFakeAvailableContent|.
const std::string GetExpectedAvailableContentAsJson() {
  // About the below data:
  // * |content_type| is an AvailableContentType enum value where
  //   0 = kPrefetchedPage and 3=kOtherPage.
  // * The base64 encoded values represent the encoded versions of the
  //   respective entries returned by |GetFakeAvailableContent|.
  std::string want_json = R"([
    {
      "ID": "ID",
      "attribution_base64": "AGEAdAB0AHIAaQBiAHUAdABpAG8Abg==",
      "content_type": 0,
      "date_modified": "date_modified",
      "favicon_data_uri": "data:image/png;base64,def",
      "name_space": "name_space",
      "snippet_base64": "AHMAbgBpAHAAcABlAHQ=",
      "thumbnail_data_uri": "data:image/png;base64,abc",
      "title_base64": "AHQAaQB0AGwAZQ=="
    },
    {
      "ID": "ID2",
      "attribution_base64": "AGEAdAB0AHIAaQBiAHUAdABpAG8AbgAy",
      "content_type": 3,
      "date_modified": "date_modified2",
      "favicon_data_uri": "data:image/png;base64,def",
      "name_space": "name_space2",
      "snippet_base64": "AHMAbgBpAHAAcABlAHQAMg==",
      "thumbnail_data_uri": "data:image/png;base64,abc",
      "title_base64": "AHQAaQB0AGwAZQAy"
    }
  ])";
  base::ReplaceChars(want_json, base::kWhitespaceASCII, "", &want_json);
  return want_json;
}

class FakeAvailableOfflineContentProvider
    : public chrome::mojom::AvailableOfflineContentProvider {
 public:
  FakeAvailableOfflineContentProvider() = default;

  FakeAvailableOfflineContentProvider(
      const FakeAvailableOfflineContentProvider&) = delete;
  FakeAvailableOfflineContentProvider& operator=(
      const FakeAvailableOfflineContentProvider&) = delete;

  void List(ListCallback callback) override {
    if (return_content_) {
      std::move(callback).Run(list_visible_by_prefs_,
                              GetFakeAvailableContent());
    } else {
      std::move(callback).Run(list_visible_by_prefs_, {});
    }
  }

  MOCK_METHOD2(LaunchItem,
               void(const std::string& item_ID, const std::string& name_space));
  MOCK_METHOD1(LaunchDownloadsPage, void(bool open_prefetched_articles_tab));
  MOCK_METHOD1(ListVisibilityChanged, void(bool is_visible));

  void AddBinding(
      mojo::PendingReceiver<chrome::mojom::AvailableOfflineContentProvider>
          receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void set_return_content(bool return_content) {
    return_content_ = return_content;
  }

  void set_list_visible_by_prefs(bool list_visible_by_prefs) {
    list_visible_by_prefs_ = list_visible_by_prefs;
  }

 private:
  bool return_content_ = true;
  bool list_visible_by_prefs_ = true;
  mojo::ReceiverSet<chrome::mojom::AvailableOfflineContentProvider> receivers_;
};

// Provides set up for testing the 'available offline content' feature.
class NetErrorHelperCoreAvailableOfflineContentTest
    : public NetErrorHelperCoreTest {
 public:
  void SetUp() override {
    NetErrorHelperCoreTest::SetUp();
    AvailableOfflineContentHelper::OverrideBinderForTesting(
        base::BindRepeating(&FakeAvailableOfflineContentProvider::AddBinding,
                            base::Unretained(&fake_provider_)));
  }

  void TearDown() override {
    AvailableOfflineContentHelper::OverrideBinderForTesting(
        base::NullCallback());
  }

 protected:
  FakeAvailableOfflineContentProvider fake_provider_;
  base::HistogramTester histogram_tester_;
};

TEST_F(NetErrorHelperCoreAvailableOfflineContentTest, ListAvailableContent) {
  set_offline_content_feature_enabled(true);
  fake_provider_.set_return_content(true);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(list_visible_by_prefs());
  EXPECT_EQ(GetExpectedAvailableContentAsJson(), offline_content_json());

  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN, 1);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED, 0);

  core()->LaunchOfflineItem("ID", "name_space");
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTION_CLICKED, 1);

  core()->LaunchDownloadsPage();
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_DOWNLOADS_PAGE_CLICKED, 1);
}

TEST_F(NetErrorHelperCoreAvailableOfflineContentTest, ListHiddenByPrefs) {
  set_offline_content_feature_enabled(true);
  fake_provider_.set_return_content(true);
  fake_provider_.set_list_visible_by_prefs(false);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(list_visible_by_prefs());
  EXPECT_EQ(GetExpectedAvailableContentAsJson(), offline_content_json());

  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN, 0);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED, 1);

  core()->LaunchOfflineItem("ID", "name_space");
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTION_CLICKED, 1);

  core()->LaunchDownloadsPage();
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_DOWNLOADS_PAGE_CLICKED, 1);
}

TEST_F(NetErrorHelperCoreAvailableOfflineContentTest, ListNoAvailableContent) {
  set_offline_content_feature_enabled(true);
  fake_provider_.set_return_content(false);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(list_visible_by_prefs());
  EXPECT_EQ("", offline_content_json());
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN, 0);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED, 0);
}

TEST_F(NetErrorHelperCoreAvailableOfflineContentTest, NotAllowed) {
  set_offline_content_feature_enabled(false);
  fake_provider_.set_return_content(true);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(list_visible_by_prefs());
  EXPECT_EQ("", offline_content_json());
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN, 0);
  histogram_tester_.ExpectBucketCount(
      "Net.ErrorPageCounts",
      error_page::NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED, 0);
}

class FakeOfflinePageAutoFetcher
    : public chrome::mojom::OfflinePageAutoFetcher {
 public:
  FakeOfflinePageAutoFetcher() = default;

  FakeOfflinePageAutoFetcher(const FakeOfflinePageAutoFetcher&) = delete;
  FakeOfflinePageAutoFetcher& operator=(const FakeOfflinePageAutoFetcher&) =
      delete;

  struct TryScheduleParameters {
    bool user_requested;
    TryScheduleCallback callback;
  };

  void TrySchedule(bool user_requested, TryScheduleCallback callback) override {
    try_schedule_calls_.push_back({user_requested, std::move(callback)});
  }

  void CancelSchedule() override { cancel_calls_++; }

  void AddReceiver(
      mojo::PendingReceiver<chrome::mojom::OfflinePageAutoFetcher> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  int cancel_calls() const { return cancel_calls_; }
  std::vector<TryScheduleParameters> take_try_schedule_calls() {
    std::vector<TryScheduleParameters> result = std::move(try_schedule_calls_);
    try_schedule_calls_.clear();
    return result;
  }

 private:
  mojo::ReceiverSet<chrome::mojom::OfflinePageAutoFetcher> receivers_;
  int cancel_calls_ = 0;
  std::vector<TryScheduleParameters> try_schedule_calls_;
};
// This uses the real implementation of PageAutoFetcherHelper, but with a
// substituted fetcher.
class TestPageAutoFetcherHelper : public PageAutoFetcherHelper {
 public:
  explicit TestPageAutoFetcherHelper(
      base::RepeatingCallback<
          mojo::PendingRemote<chrome::mojom::OfflinePageAutoFetcher>()> binder)
      : PageAutoFetcherHelper(nullptr), binder_(binder) {}
  bool Bind() override {
    if (!fetcher_)
      fetcher_.Bind(binder_.Run());
    return true;
  }

 private:
  base::RepeatingCallback<
      mojo::PendingRemote<chrome::mojom::OfflinePageAutoFetcher>()>
      binder_;
};

// Provides set up for testing the 'auto fetch on dino' feature.
class NetErrorHelperCoreAutoFetchTest : public NetErrorHelperCoreTest {
 public:
  void SetUp() override {
    NetErrorHelperCoreTest::SetUp();
    auto binder = base::BindLambdaForTesting([&]() {
      mojo::PendingRemote<chrome::mojom::OfflinePageAutoFetcher> fetcher_remote;
      fake_fetcher_.AddReceiver(
          fetcher_remote.InitWithNewPipeAndPassReceiver());
      return fetcher_remote;
    });

    core()->SetPageAutoFetcherHelperForTesting(
        std::make_unique<TestPageAutoFetcherHelper>(binder));
  }

  void TearDown() override {
    AvailableOfflineContentHelper::OverrideBinderForTesting(
        base::NullCallback());
  }

 protected:
  FakeOfflinePageAutoFetcher fake_fetcher_;
};

TEST_F(NetErrorHelperCoreAutoFetchTest, NotAllowed) {
  set_auto_fetch_allowed(false);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  // When auto fetch is not allowed, OfflinePageAutoFetcher is not called.
  std::vector<FakeOfflinePageAutoFetcher::TryScheduleParameters> calls =
      fake_fetcher_.take_try_schedule_calls();
  EXPECT_EQ(0ul, calls.size());
}

TEST_F(NetErrorHelperCoreAutoFetchTest, AutoFetchTriggered) {
  set_auto_fetch_allowed(true);

  DoErrorLoad(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  // Auto fetch is allowed, so OfflinePageAutoFetcher is called once.
  std::vector<FakeOfflinePageAutoFetcher::TryScheduleParameters> calls =
      fake_fetcher_.take_try_schedule_calls();
  EXPECT_EQ(1ul, calls.size());

  // Finalize the call to TrySchedule, and verify the delegate is called.
  std::move(calls[0].callback)
      .Run(chrome::mojom::OfflinePageAutoFetcherScheduleResult::kScheduled);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(chrome::mojom::OfflinePageAutoFetcherScheduleResult::kScheduled,
            auto_fetch_state());
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
