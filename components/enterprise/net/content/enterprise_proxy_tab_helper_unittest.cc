// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_net {

namespace {

class MockTabHelperDelegate : public EnterpriseProxyTabHelper::Delegate {
 public:
  ~MockTabHelperDelegate() override = default;

  MOCK_METHOD(void, SignIn, (content::WebContents*), (override));
};

}  // namespace

class EnterpriseProxyTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  EnterpriseProxyTabHelperTest() {
    feature_list_.InitAndEnableFeature(kEnterpriseProxyErrorHandling);
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    error_service_ =
        std::make_unique<EnterpriseProxyErrorService>(&mock_proxy_service_);
    tab_helper_ = std::make_unique<EnterpriseProxyTabHelper>(
        mock_tab_, web_contents(), error_service_.get());
  }

  void TearDown() override {
    tab_helper_.reset();
    content::RenderViewHostTestHarness::TearDown();
    error_service_.reset();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  tabs::MockTabInterface mock_tab_;
  MockEnterpriseProxyService mock_proxy_service_;
  std::unique_ptr<EnterpriseProxyErrorService> error_service_;
  std::unique_ptr<EnterpriseProxyTabHelper> tab_helper_;
};

TEST_F(EnterpriseProxyTabHelperTest, FromTabInterface) {
  EXPECT_EQ(EnterpriseProxyTabHelper::From(&mock_tab_), tab_helper_.get());
  EXPECT_EQ(EnterpriseProxyTabHelper::From(nullptr), nullptr);
}

TEST_F(EnterpriseProxyTabHelperTest,
       PrimaryMainFrameNavigation_TracksIdAndCleansUpErrorState) {
  ASSERT_TRUE(tab_helper_);
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://target.example.com"), web_contents());
  simulator->Start();

  int64_t nav_id = tab_helper_->active_navigation_id();
  EXPECT_NE(nav_id, 0);

  net::RecordingNetLogObserver observer;
  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);

  EnterpriseProxyErrorData error_data(GURL("https://target.example.com"),
                                      GURL("https://proxy.example.com"), 502);
  error_service_->RecordDisguisedError(nav_id, error_data, net_log);
  EXPECT_TRUE(error_service_->TakeDisguisedError(nav_id).has_value());

  auto saved_entries = observer.GetEntriesWithType(
      net::NetLogEventType::ENTERPRISE_PROXY_DISGUISED_ERROR_SAVED);
  ASSERT_EQ(1u, saved_entries.size());
  EXPECT_EQ(net_log.source().id, saved_entries[0].source.id);
  EXPECT_EQ(base::NumberToString(nav_id),
            *saved_entries[0].params.FindString("navigation_id"));
  EXPECT_EQ("https://target.example.com/",
            *saved_entries[0].params.FindString("destination_url"));
  EXPECT_EQ("https://proxy.example.com/",
            *saved_entries[0].params.FindString("proxy_url"));
  EXPECT_EQ(502, saved_entries[0].params.FindInt("error_code"));

  // Record again so we can verify cleanup on DidFinishNavigation.
  error_service_->RecordDisguisedError(nav_id, error_data, net_log);

  simulator->Fail(net::ERR_TUNNEL_CONNECTION_FAILED);
  simulator->CommitErrorPage();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);
  EXPECT_FALSE(error_service_->TakeDisguisedError(nav_id).has_value());
}

TEST_F(EnterpriseProxyTabHelperTest, IgnoresSubframeNavigations) {
  ASSERT_TRUE(tab_helper_);
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  auto subframe_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://subframe.example.com"), child_rfh);
  subframe_simulator->Start();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  subframe_simulator->Commit();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);
}

TEST_F(EnterpriseProxyTabHelperTest, IgnoresSameDocumentNavigations) {
  ASSERT_TRUE(tab_helper_);
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  // Commit an initial cross-document navigation to set up the document.
  auto initial_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://target.example.com"), web_contents());
  initial_simulator->Commit();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  // Same-document navigation should not update active_navigation_id.
  auto same_doc_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://target.example.com#hash"), main_rfh());
  same_doc_simulator->CommitSameDocument();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);
}

TEST_F(EnterpriseProxyTabHelperTest, SignIn_InvokesDelegate) {
  ASSERT_TRUE(tab_helper_);

  auto delegate = std::make_unique<MockTabHelperDelegate>();
  EXPECT_CALL(*delegate, SignIn(web_contents()));
  tab_helper_->SetDelegateForTesting(std::move(delegate));

  tab_helper_->SignIn();
}

TEST_F(EnterpriseProxyTabHelperTest,
       DisguisedErrorRedirectUrl_RedirectsOnceAndPreventsLoop) {
  const GURL previous_url("https://previous.example.com/");
  const GURL referrer_url("https://referrer.example.com/page");
  const GURL blocked_url("https://target.example.com/secret");
  const GURL static_error_url(
      "https://error.example.com/blocked.html?txid=test-txid-1");

  auto initial_page = content::NavigationSimulator::CreateBrowserInitiated(
      previous_url, web_contents());
  initial_page->Commit();

  content::NavigationController::LoadURLParams blocked_params(blocked_url);
  blocked_params.referrer =
      content::Referrer(referrer_url, network::mojom::ReferrerPolicy::kDefault);
  web_contents()->GetController().LoadURLWithParams(blocked_params);
  ASSERT_TRUE(web_contents()->GetController().GetPendingEntry());
  ASSERT_EQ(
      referrer_url,
      web_contents()->GetController().GetPendingEntry()->GetReferrer().url);
  const int64_t initial_nav_id = tab_helper_->active_navigation_id();
  ASSERT_NE(initial_nav_id, 0);

  base::HistogramTester histogram_tester;
  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);
  EnterpriseProxyErrorData error_data(
      blocked_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
      static_error_url);
  error_service_->RecordDisguisedError(initial_nav_id, error_data, net_log);

  // Disguised error for `initial_nav_id` is consumed so the built-in error page
  // does not claim it, and the posted task starts navigating to
  // `static_error_url`.
  EXPECT_FALSE(error_service_->TakeDisguisedError(initial_nav_id).has_value());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    content::NavigationEntry* entry =
        web_contents()->GetController().GetPendingEntry();
    return entry && entry->GetURL() == static_error_url;
  }));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
      SecureGatewayStaticErrorPageRedirectOutcome::kRedirected, 1);

  content::NavigationEntry* pending_entry =
      web_contents()->GetController().GetPendingEntry();
  ASSERT_TRUE(pending_entry);
  EXPECT_EQ(static_error_url, pending_entry->GetURL());
  EXPECT_TRUE(pending_entry->GetReferrer().url.is_empty());
  EXPECT_EQ(static_error_url, web_contents()->GetVisibleURL());

  const int64_t redirect_nav_id = tab_helper_->active_navigation_id();
  EXPECT_NE(redirect_nav_id, 0);
  EXPECT_NE(redirect_nav_id, initial_nav_id);

  // If the static error page navigation itself hits another disguised-error
  // `url=`, loop protection must NOT redirect a second time and must keep the
  // disguised error stored for the built-in interstitial fallback.
  EnterpriseProxyErrorData loop_error_data(
      static_error_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
      GURL("https://error.example.com/blocked.html?txid=test-txid-2"));
  error_service_->RecordDisguisedError(redirect_nav_id, loop_error_data,
                                       net_log);
  EXPECT_TRUE(error_service_->TakeDisguisedError(redirect_nav_id).has_value());
  histogram_tester.ExpectBucketCount(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
      SecureGatewayStaticErrorPageRedirectOutcome::kRejectedLoopDetected, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome", 2);

  // Commit the pending redirect navigation and verify that the previous page
  // (committed without user activation) was NOT marked skippable on Back.
  std::unique_ptr<content::NavigationSimulator> redirect_simulator =
      content::NavigationSimulator::CreateFromPending(
          web_contents()->GetController());
  ASSERT_TRUE(redirect_simulator);
  redirect_simulator->Commit();
  EXPECT_EQ(static_error_url, web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(web_contents()->GetController().CanGoBack());
  ASSERT_NE(nullptr, web_contents()->GetController().GetEntryAtOffset(-1));
  EXPECT_EQ(previous_url,
            web_contents()->GetController().GetEntryAtOffset(-1)->GetURL());

  // Also verify clean observer detachment on KeyedService::Shutdown().
  error_service_->Shutdown();
}

TEST_F(EnterpriseProxyTabHelperTest,
       DisguisedErrorRedirectUrl_DisabledByFeatureParam) {
  base::test::ScopedFeatureList disabled_redirect_feature;
  disabled_redirect_feature.InitAndEnableFeatureWithParameters(
      kEnterpriseProxyErrorHandling,
      {{kEnableStaticErrorPageRedirectParamName, "false"}});

  base::HistogramTester histogram_tester;
  const GURL blocked_url("https://target.example.com/secret");
  const GURL static_error_url(
      "https://error.example.com/blocked.html?txid=test-txid-disabled");

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      blocked_url, web_contents());
  simulator->Start();
  const int64_t nav_id = tab_helper_->active_navigation_id();
  ASSERT_NE(nav_id, 0);

  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);
  EnterpriseProxyErrorData error_data(
      blocked_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
      static_error_url);
  error_service_->RecordDisguisedError(nav_id, error_data, net_log);

  // When the redirect param is disabled, `error_data` remains available for the
  // built-in error interstitial and no redirect navigation is started.
  EXPECT_TRUE(error_service_->TakeDisguisedError(nav_id).has_value());
  base::RunLoop flush_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_loop.QuitClosure());
  flush_loop.Run();
  ASSERT_NE(nullptr, web_contents()->GetController().GetPendingEntry());
  EXPECT_EQ(blocked_url,
            web_contents()->GetController().GetPendingEntry()->GetURL());
  histogram_tester.ExpectTotalCount(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome", 0);
}

TEST_F(EnterpriseProxyTabHelperTest,
       DisguisedErrorRedirectUrl_DisabledWhenParentFeatureDisabled) {
  // Even though `kEnableStaticErrorPageRedirectParam` defaults to `true`,
  // disabling `kEnterpriseProxyErrorHandling` (the Desktop default-off state)
  // must keep `IsStaticErrorPageRedirectEnabled()` false and prevent
  // redirection.
  base::test::ScopedFeatureList disabled_parent_feature;
  disabled_parent_feature.InitAndDisableFeature(kEnterpriseProxyErrorHandling);
  ASSERT_TRUE(kEnableStaticErrorPageRedirectParam.Get());
  EXPECT_FALSE(IsStaticErrorPageRedirectEnabled());

  base::HistogramTester histogram_tester;
  const GURL blocked_url("https://target.example.com/secret");
  const GURL static_error_url(
      "https://error.example.com/blocked.html?txid=test-txid-parent-disabled");

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      blocked_url, web_contents());
  simulator->Start();
  const int64_t nav_id = tab_helper_->active_navigation_id();
  ASSERT_NE(nav_id, 0);

  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);
  EnterpriseProxyErrorData error_data(
      blocked_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
      static_error_url);
  error_service_->RecordDisguisedError(nav_id, error_data, net_log);

  EXPECT_TRUE(error_service_->TakeDisguisedError(nav_id).has_value());
  base::RunLoop flush_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_loop.QuitClosure());
  flush_loop.Run();
  ASSERT_NE(nullptr, web_contents()->GetController().GetPendingEntry());
  EXPECT_EQ(blocked_url,
            web_contents()->GetController().GetPendingEntry()->GetURL());
  histogram_tester.ExpectTotalCount(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome", 0);
}

TEST_F(EnterpriseProxyTabHelperTest,
       DisguisedErrorRedirectUrl_RejectsSelfRedirectIncludingFragment) {
  base::HistogramTester histogram_tester;
  const GURL blocked_url("https://target.example.com/secret#section1");

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      blocked_url, web_contents());
  simulator->Start();
  const int64_t nav_id = tab_helper_->active_navigation_id();
  ASSERT_NE(nav_id, 0);

  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);

  // Exact self-redirect (`redirect_url == destination_url`) is rejected.
  EnterpriseProxyErrorData exact_self_redirect(
      blocked_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization, blocked_url);
  error_service_->RecordDisguisedError(nav_id, exact_self_redirect, net_log);
  EXPECT_TRUE(error_service_->TakeDisguisedError(nav_id).has_value());

  // Self-redirect differing only in `#fragment` (`EqualsIgnoringRef`) is also
  // rejected so it cannot re-navigate to the same blocked resource.
  EnterpriseProxyErrorData fragment_self_redirect(
      blocked_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
      GURL("https://target.example.com/secret#other-section"));
  error_service_->RecordDisguisedError(nav_id, fragment_self_redirect, net_log);
  EXPECT_TRUE(error_service_->TakeDisguisedError(nav_id).has_value());

  base::RunLoop flush_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_loop.QuitClosure());
  flush_loop.Run();
  ASSERT_NE(nullptr, web_contents()->GetController().GetPendingEntry());
  EXPECT_EQ(blocked_url,
            web_contents()->GetController().GetPendingEntry()->GetURL());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
      SecureGatewayStaticErrorPageRedirectOutcome::kRejectedLoopDetected, 2);
}

TEST_F(EnterpriseProxyTabHelperTest,
       DisguisedErrorRedirectUrl_ChecksDestinationUrlAndFollowsRedirects) {
  base::HistogramTester histogram_tester;
  const GURL start_url("https://initial.example.com/start");
  const GURL redirected_url("https://target.example.com/blocked");
  const GURL unrelated_subresource_url("https://cdn.example.com/script.js");
  const GURL static_error_url(
      "https://error.example.com/blocked.html?txid=test-txid-redirect");

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      start_url, web_contents());
  simulator->Start();
  const int64_t nav_id = tab_helper_->active_navigation_id();
  ASSERT_NE(nav_id, 0);

  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);

  // A disguised error for an unrelated URL (e.g. a ServiceWorker subresource
  // fetch attributed to `nav_id`) must not hijack the main-frame navigation.
  EnterpriseProxyErrorData mismatched_error(
      unrelated_subresource_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
      static_error_url);
  error_service_->RecordDisguisedError(nav_id, mismatched_error, net_log);
  EXPECT_TRUE(error_service_->TakeDisguisedError(nav_id).has_value());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
      SecureGatewayStaticErrorPageRedirectOutcome::kRejectedDestinationMismatch,
      1);

  // After an HTTP redirect updates the navigation URL, a matching disguised
  // error for `redirected_url` triggers the static error page redirect.
  simulator->Redirect(redirected_url);
  EnterpriseProxyErrorData matching_error(
      redirected_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
      static_error_url);
  error_service_->RecordDisguisedError(nav_id, matching_error, net_log);
  EXPECT_FALSE(error_service_->TakeDisguisedError(nav_id).has_value());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    content::NavigationEntry* entry =
        web_contents()->GetController().GetPendingEntry();
    return entry && entry->GetURL() == static_error_url;
  }));
  histogram_tester.ExpectBucketCount(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
      SecureGatewayStaticErrorPageRedirectOutcome::kRedirected, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome", 2);
}

TEST_F(EnterpriseProxyTabHelperTest,
       DisguisedErrorRedirectUrl_CanceledWhenNewerNavigationStarts) {
  base::HistogramTester histogram_tester;
  const GURL blocked_url("https://target.example.com/secret");
  const GURL static_error_url(
      "https://error.example.com/blocked.html?txid=test-txid-canceled");
  const GURL newer_url("https://other.example.com/page");

  auto first_nav = content::NavigationSimulator::CreateBrowserInitiated(
      blocked_url, web_contents());
  first_nav->Start();
  const int64_t first_nav_id = tab_helper_->active_navigation_id();
  ASSERT_NE(first_nav_id, 0);

  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);
  EnterpriseProxyErrorData error_data(
      blocked_url, GURL("https://proxy.example.com"), 403,
      EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
      static_error_url);
  error_service_->RecordDisguisedError(first_nav_id, error_data, net_log);
  EXPECT_FALSE(error_service_->TakeDisguisedError(first_nav_id).has_value());

  // Starting a newer navigation before the posted `RedirectToStaticErrorPage`
  // task runs updates `active_navigation_id_` and cancels the stale redirect.
  auto second_nav = content::NavigationSimulator::CreateBrowserInitiated(
      newer_url, web_contents());
  second_nav->Start();
  EXPECT_NE(tab_helper_->active_navigation_id(), first_nav_id);

  base::RunLoop flush_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_loop.QuitClosure());
  flush_loop.Run();

  ASSERT_NE(nullptr, web_contents()->GetController().GetPendingEntry());
  EXPECT_EQ(newer_url,
            web_contents()->GetController().GetPendingEntry()->GetURL());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
      SecureGatewayStaticErrorPageRedirectOutcome::kCanceledBeforeRedirect, 1);
}

TEST_F(EnterpriseProxyTabHelperTest,
       DisguisedErrorRedirectUrl_RejectsInvalidUrlsAtNavigationSink) {
  base::HistogramTester histogram_tester;
  const GURL blocked_url("https://target.example.com/secret");

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      blocked_url, web_contents());
  simulator->Start();
  const int64_t nav_id = tab_helper_->active_navigation_id();
  ASSERT_NE(nav_id, 0);

  net::NetLogWithSource net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);

  const GURL kInvalidRedirectUrls[] = {
      GURL("not a valid url"),
      GURL("http://error.example.com/blocked.html"),
      GURL("javascript:alert(1)"),
      GURL("https://user:pass@error.example.com/blocked.html"),
  };

  for (const GURL& bad_redirect_url : kInvalidRedirectUrls) {
    SCOPED_TRACE(bad_redirect_url.possibly_invalid_spec());
    EnterpriseProxyErrorData invalid_error(
        blocked_url, GURL("https://proxy.example.com"), 403,
        EnterpriseProxyErrorData::ErrorCategory::kAuthorization,
        bad_redirect_url);
    error_service_->RecordDisguisedError(nav_id, invalid_error, net_log);
    // Error data remains stored so Chrome falls back to the built-in error
    // interstitial.
    EXPECT_TRUE(error_service_->TakeDisguisedError(nav_id).has_value());
  }

  base::RunLoop flush_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, flush_loop.QuitClosure());
  flush_loop.Run();

  ASSERT_NE(nullptr, web_contents()->GetController().GetPendingEntry());
  EXPECT_EQ(blocked_url,
            web_contents()->GetController().GetPendingEntry()->GetURL());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.SecureGateway.StaticErrorPageRedirectOutcome",
      SecureGatewayStaticErrorPageRedirectOutcome::kRejectedInvalidUrl, 4);
}

}  // namespace enterprise_net
