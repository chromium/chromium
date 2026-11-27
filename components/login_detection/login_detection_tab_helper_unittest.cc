// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/login_detection/login_detection_tab_helper.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/login_detection/login_detection_prefs.h"
#include "components/login_detection/login_detection_type.h"
#include "components/login_detection/login_detection_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace login_detection {

class LoginDetectionTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  LoginDetectionTabHelperTest() = default;

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    auto browser_context =
        std::make_unique<user_prefs::TestBrowserContextWithPrefs>();
    prefs::RegisterProfilePrefs(browser_context->pref_registry());
    return browser_context;
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kLoginDetection);
    content::RenderViewHostTestHarness::SetUp();
    ResetMetricsTesters();
    LoginDetectionTabHelper::MaybeCreateForWebContents(
        web_contents(), federated_login_callback_.Get());
  }

  void ResetMetricsTesters() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Verifies the metrics for the given login detection type to be recorded.
  void VerifyLoginDetectionTypeMetrics(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);
    VerifyLoginDetectionUkm(type, ukm::builders::LoginDetectionV2::kEntryName);
    VerifyLoginDetectionUkm(
        type, ukm::builders::LoginDetectionV2IdentityProvider::kEntryName);
  }

  void VerifyLoginDetectionUkm(LoginDetectionType type,
                               const char* entry_name) {
    auto entries = ukm_recorder_->GetEntriesByName(entry_name);
    if (type == LoginDetectionType::kNoLogin) {
      EXPECT_TRUE(entries.empty());
      return;
    }

    ASSERT_EQ(1u, entries.size());
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entries[0], ukm::builders::LoginDetectionV2::kPage_LoginTypeName,
        static_cast<int64_t>(type));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  testing::NiceMock<
      base::MockCallback<LoginDetectionTabHelper::FederatedLoginCallback>>
      federated_login_callback_;
};

TEST_F(LoginDetectionTabHelperTest, NoLogin) {
  EXPECT_CALL(federated_login_callback_, Run(testing::_)).Times(0);

  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
}

TEST_F(LoginDetectionTabHelperTest, SimpleOAuthLogin) {
  EXPECT_CALL(federated_login_callback_, Run(testing::_)).Times(0);

  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // OAuth login start
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);

  // OAuth login complete
  ResetMetricsTesters();
  testing::Mock::VerifyAndClearExpectations(&federated_login_callback_);
  EXPECT_CALL(federated_login_callback_, Run(web_contents())).Times(1);
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

TEST_F(LoginDetectionTabHelperTest, NavigationToOAuthLoggedInSite) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // Trigger OAuth login so that the site is saved in prefs.
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));

  // Navigation to a non logged-in site.
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://bar.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
}

TEST_F(LoginDetectionTabHelperTest, OAuthLoginViaRedirect) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // OAuth login start and complete via redirects.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://foo.com/oauth_signin"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  simulator->Redirect(GURL("https://oauth.com/authenticate?client_id=123"));
  simulator->Redirect(GURL("https://oauth.com/user_login"));
  simulator->Redirect(GURL("https://oauth.com/?username=user&password=123"));
  simulator->Redirect(GURL("https://foo.com/redirect?code=secret"));
  simulator->Commit();

  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

// Test that OAuth login is still detected when there are intermediate
// navigations to other sites.
TEST_F(LoginDetectionTabHelperTest, InvalidOAuthLogins) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // OAuth login start
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);

  // Intermediate navigation just ignored
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://bar.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);

  // OAuth login complete will be detected
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

// Test that OAuth login is still detected when there are intermediate redirect
// navigations to other sites.
TEST_F(LoginDetectionTabHelperTest, InvalidOAuthLoginsWithRedirect) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // OAuth login start and complete via redirects.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://foo.com/oauth_signin"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  simulator->Redirect(GURL("https://oauth.com/authenticate?client_id=123"));
  simulator->Redirect(GURL("https://oauth.com/user_login"));
  simulator->Redirect(GURL("https://oauth.com/?username=user&password=123"));
  simulator->Redirect(GURL("https://bar.com/page.html"));
  simulator->Redirect(GURL("https://foo.com/redirect?code=secret"));
  simulator->Commit();

  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

TEST_F(LoginDetectionTabHelperTest, PopUpOAuthLogin) {
  EXPECT_CALL(federated_login_callback_, Run(testing::_)).Times(0);

  // Opener page.
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // Simulate opening a popup before tab helpers are initialized on it.
  std::unique_ptr<content::WebContents> popup_web_contents =
      CreateTestWebContents();
  LoginDetectionTabHelper* opener_tab_helper =
      LoginDetectionTabHelper::FromWebContents(web_contents());
  opener_tab_helper->DidOpenRequestedURL(
      popup_web_contents.get(), web_contents()->GetPrimaryMainFrame(),
      GURL("https://oauth.com/authenticate?client_id=123"), content::Referrer(),
      ui::mojom::WindowOpenDisposition::kNewPopup, ui::PAGE_TRANSITION_LINK,
      false, true);

  // Navigate the popup.
  auto popup_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://oauth.com/authenticate?client_id=123"),
      popup_web_contents.get());
  popup_simulator->Commit();

  // The popup navigation should not record any login metric.
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // Destroying the popup should trigger login detection.
  testing::Mock::VerifyAndClearExpectations(&federated_login_callback_);
  EXPECT_CALL(federated_login_callback_, Run(popup_web_contents.get()))
      .Times(1);
  popup_web_contents.reset();
  VerifyLoginDetectionTypeMetrics(
      LoginDetectionType::kOauthPopUpFirstTimeLoginFlow);
}

}  // namespace login_detection
