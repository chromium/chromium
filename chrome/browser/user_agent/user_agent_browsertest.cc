// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/features.h"

namespace policy {

using EnterprisePolicyState =
    ChromeContentBrowserClient::UserAgentReductionEnterprisePolicyState;

class UserAgentBrowserTest : public InProcessBrowserTest,
                             public testing::WithParamInterface<bool> {
 public:
  UserAgentBrowserTest() {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &UserAgentBrowserTest::MonitorUserAgent, base::Unretained(this)));
  }

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kReduceUserAgent);
    }

    ASSERT_TRUE(embedded_test_server()->Start());
    empty_url_ = embedded_test_server()->GetURL("/empty.html");

    InProcessBrowserTest::SetUp();
  }

  void set_user_agent_reduction_policy(int policy) {
    browser()->profile()->GetPrefs()->SetInteger(prefs::kUserAgentReduction,
                                                 policy);
  }

  int user_agent_reduction_policy() {
    return browser()->profile()->GetPrefs()->GetInteger(
        prefs::kUserAgentReduction);
  }

  std::string observed_user_agent() { return observered_user_agent_; }

  GURL empty_url() { return empty_url_; }

 private:
  void MonitorUserAgent(const net::test_server::HttpRequest& request) {
    if (request.GetURL() == empty_url()) {
      observered_user_agent_ =
          request.headers.find(net::HttpRequestHeaders::kUserAgent)->second;
    }
  }

  std::string observered_user_agent_;
  base::test::ScopedFeatureList scoped_feature_list_;
  GURL empty_url_;
};

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, EnterprisePolicyInitialized) {
  // Check that default is set correctly
  EXPECT_EQ(EnterprisePolicyState::kDefault, user_agent_reduction_policy());
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, EnterprisePolicyDisabled) {
  set_user_agent_reduction_policy(EnterprisePolicyState::kForceDisabled);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(), embedder_support::GetFullUserAgent());
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, EnterprisePolicyEnabled) {
  set_user_agent_reduction_policy(EnterprisePolicyState::kForceEnabled);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(), embedder_support::GetReducedUserAgent());
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, EnterprisePolicyDefault) {
  set_user_agent_reduction_policy(EnterprisePolicyState::kDefault);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(), embedder_support::GetUserAgent());
}

INSTANTIATE_TEST_SUITE_P(ReduceUserAgentFeature,
                         UserAgentBrowserTest,
                         ::testing::Bool());

}  // namespace policy
