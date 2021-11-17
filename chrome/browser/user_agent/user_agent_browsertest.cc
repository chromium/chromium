// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
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
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kReduceUserAgent);
    }
  }

  void ExpectUserAgent(std::string expected_user_agent) {
    EXPECT_EQ(expected_user_agent, observered_user_agent_);
  }

  void set_user_agent_reduction_policy(int policy) {
    browser()->profile()->GetPrefs()->SetInteger(prefs::kUserAgentReduction,
                                                 policy);
  }

  int user_agent_reduction_policy() {
    return browser()->profile()->GetPrefs()->GetInteger(
        prefs::kUserAgentReduction);
  }

 private:
  void MonitorUserAgent(const net::test_server::HttpRequest& request) {
    observered_user_agent_ =
        request.headers.find(net::HttpRequestHeaders::kUserAgent)->second;
  }

  std::string observered_user_agent_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, EnterprisePolicyState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/empty.html");

  // Check that default is set correctly
  EXPECT_EQ(EnterprisePolicyState::kDefault, user_agent_reduction_policy());

  set_user_agent_reduction_policy(EnterprisePolicyState::kForceDisabled);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ExpectUserAgent(embedder_support::GetFullUserAgent());

  set_user_agent_reduction_policy(EnterprisePolicyState::kForceEnabled);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ExpectUserAgent(embedder_support::GetReducedUserAgent());

  set_user_agent_reduction_policy(EnterprisePolicyState::kDefault);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ExpectUserAgent(embedder_support::GetUserAgent());
}

INSTANTIATE_TEST_SUITE_P(ReduceUserAgentFeature,
                         UserAgentBrowserTest,
                         ::testing::Bool());

}  // namespace policy
