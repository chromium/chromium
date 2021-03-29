// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/renderer/subresource_redirect/login_robots_decider_agent.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace subresource_redirect {

// Helper class that exposes a callback for the decider to call, and maintains
// the redirect result for verification.
class RedirectResultReceiver {
 public:
  PublicResourceDecider::ShouldRedirectDecisionCallback GetCallback() {
    return base::BindOnce(
        &RedirectResultReceiver::OnShouldRedirectDecisionCallback,
        weak_ptr_factory_.GetWeakPtr());
  }

  SubresourceRedirectResult subresource_redirect_result() const {
    return redirect_result_;
  }

  bool did_receive_result() const { return did_receive_result_; }

 private:
  void OnShouldRedirectDecisionCallback(
      SubresourceRedirectResult redirect_result) {
    EXPECT_FALSE(did_receive_result_);
    did_receive_result_ = true;
    redirect_result_ = redirect_result;
  }

  SubresourceRedirectResult redirect_result_;
  bool did_receive_result_ = false;
  base::WeakPtrFactory<RedirectResultReceiver> weak_ptr_factory_{this};
};

class SubresourceRedirectLoginRobotsDeciderAgentTest
    : public ChromeRenderViewTest {
 public:
  void SetUpRobotsRules(const std::string& origin,
                        const std::vector<RobotsRule>& patterns) {
    login_robots_decider_agent_->UpdateRobotsRulesForTesting(
        url::Origin::Create(GURL(origin)), GetRobotsRulesProtoString(patterns));
  }

  bool ShouldRedirectSubresource(const std::string& url) {
    RedirectResultReceiver result_receiver;
    auto immediate_result =
        login_robots_decider_agent_->ShouldRedirectSubresource(
            GURL(url), result_receiver.GetCallback());
    if (immediate_result) {
      // When the reult was sent immediately, callback should not be invoked.
      EXPECT_FALSE(result_receiver.did_receive_result());
      return *immediate_result == SubresourceRedirectResult::kRedirectable;
    }
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
    return result_receiver.did_receive_result() &&
           result_receiver.subresource_redirect_result() ==
               SubresourceRedirectResult::kRedirectable;
  }

  void SetLoggedInState(bool is_logged_in) {
    login_robots_decider_agent_->SetLoggedInState(is_logged_in);
  }

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSubresourceRedirect,
          {{"enable_login_robots_based_compression", "true"},
           {"enable_public_image_hints_based_compression", "false"}}}},
        {});
    login_robots_decider_agent_ = new LoginRobotsDeciderAgent(
        &associated_interfaces_, view_->GetMainRenderFrame());
  }

  LoginRobotsDeciderAgent* login_robots_decider_agent_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SubresourceRedirectLoginRobotsDeciderAgentTest,
       TestAllowDisallowSingleOrigin) {
  SetLoggedInState(false);
  SetUpRobotsRules("https://foo.com", {{kRuleTypeAllow, "/public"},
                                       {kRuleTypeDisallow, "/private"}});
  EXPECT_TRUE(ShouldRedirectSubresource("https://foo.com/public.jpg"));
  EXPECT_FALSE(ShouldRedirectSubresource("https://foo.com/private.jpg"));

  EXPECT_FALSE(ShouldRedirectSubresource("https://m.foo.com/public.jpg"));
  EXPECT_FALSE(ShouldRedirectSubresource("https://www.foo.com/public.jpg"));
  EXPECT_FALSE(ShouldRedirectSubresource("https://bar.com/public.jpg"));
}

TEST_F(SubresourceRedirectLoginRobotsDeciderAgentTest,
       TestHTTPRulesAreSeparate) {
  SetLoggedInState(false);
  SetUpRobotsRules("https://foo.com", {{kRuleTypeAllow, "/public"},
                                       {kRuleTypeDisallow, "/private"}});
  EXPECT_FALSE(ShouldRedirectSubresource("http://foo.com/public.jpg"));

  SetUpRobotsRules("http://foo.com", {{kRuleTypeAllow, "/public"},
                                      {kRuleTypeDisallow, "/private"}});
  EXPECT_TRUE(ShouldRedirectSubresource("http://foo.com/public.jpg"));

  EXPECT_FALSE(ShouldRedirectSubresource("http://m.foo.com/public.jpg"));
  EXPECT_FALSE(ShouldRedirectSubresource("http://www.foo.com/public.jpg"));
}

TEST_F(SubresourceRedirectLoginRobotsDeciderAgentTest, TestURLWithArguments) {
  SetLoggedInState(false);
  SetUpRobotsRules("https://foo.com",
                   {{kRuleTypeAllow, "/*.jpg$"},
                    {kRuleTypeDisallow, "/*.png?*arg_disallowed"},
                    {kRuleTypeAllow, "/*.png"},
                    {kRuleTypeDisallow, "/*.gif?*arg_disallowed"},
                    {kRuleTypeAllow, "/*.gif"},
                    {kRuleTypeDisallow, "/"}});
  EXPECT_TRUE(ShouldRedirectSubresource("https://foo.com/allowed.jpg"));
  EXPECT_TRUE(ShouldRedirectSubresource("https://foo.com/allowed.png"));
  EXPECT_TRUE(ShouldRedirectSubresource("https://foo.com/allowed.gif"));
  EXPECT_FALSE(ShouldRedirectSubresource("https://foo.com/disallowed.jpg?arg"));
  EXPECT_TRUE(ShouldRedirectSubresource("https://foo.com/allowed.png?arg"));
  EXPECT_TRUE(
      ShouldRedirectSubresource("https://foo.com/allowed.png?arg_allowed"));
  EXPECT_TRUE(ShouldRedirectSubresource(
      "https://foo.com/allowed.png?arg_allowed&arg_alllowed2"));
  EXPECT_FALSE(ShouldRedirectSubresource(
      "https://foo.com/disallowed.png?arg_disallowed"));
  EXPECT_FALSE(ShouldRedirectSubresource(
      "https://foo.com/disallowed.png?arg_disallowed&arg_disallowed2"));
}

TEST_F(SubresourceRedirectLoginRobotsDeciderAgentTest,
       TestRulesAreCaseSensitive) {
  SetLoggedInState(false);
  SetUpRobotsRules("https://foo.com", {{kRuleTypeAllow, "/allowed"},
                                       {kRuleTypeAllow, "/CamelCase"},
                                       {kRuleTypeAllow, "/CAPITALIZE"},
                                       {kRuleTypeDisallow, "/"}});
  EXPECT_TRUE(ShouldRedirectSubresource("https://foo.com/allowed.jpg"));
  EXPECT_TRUE(ShouldRedirectSubresource("https://foo.com/CamelCase.jpg"));
  EXPECT_TRUE(ShouldRedirectSubresource("https://foo.com/CAPITALIZE.jpg"));
  EXPECT_FALSE(ShouldRedirectSubresource("https://foo.com/Allowed.jpg"));
  EXPECT_FALSE(ShouldRedirectSubresource("https://foo.com/camelcase.jpg"));
  EXPECT_FALSE(ShouldRedirectSubresource("https://foo.com/capitalize.jpg"));
}

TEST_F(SubresourceRedirectLoginRobotsDeciderAgentTest,
       TestDisabledWhenLoggedIn) {
  SetLoggedInState(true);
  SetUpRobotsRules("https://foo.com", {{kRuleTypeAllow, "/public"},
                                       {kRuleTypeDisallow, "/private"}});
  EXPECT_FALSE(ShouldRedirectSubresource("https://foo.com/public.jpg"));
  EXPECT_FALSE(ShouldRedirectSubresource("https://foo.com/private.jpg"));
}

}  // namespace subresource_redirect
