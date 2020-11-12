// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/renderer/subresource_redirect/robots_rules_decider.h"
#include "components/data_reduction_proxy/proto/robots_rules.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace subresource_redirect {

namespace {

constexpr char kTestOrigin[] = "https://test.com";

const bool kRuleTypeAllow = true;
const bool kRuleTypeDisallow = false;

struct Rule {
  Rule(bool rule_type, std::string pattern)
      : rule_type(rule_type), pattern(pattern) {}

  bool rule_type;
  std::string pattern;
};

class CheckResultReceiver {
 public:
  void OnCheckRobotsRulesResult(RobotsRulesDecider::CheckResult check_result) {
    EXPECT_FALSE(did_receive_result_);
    did_receive_result_ = true;
    check_result_ = check_result;
  }
  RobotsRulesDecider::CheckResultCallback GetCallback() {
    return base::BindOnce(&CheckResultReceiver::OnCheckRobotsRulesResult,
                          weak_ptr_factory_.GetWeakPtr());
  }
  RobotsRulesDecider::CheckResult check_result() const { return check_result_; }
  bool did_receive_result() const { return did_receive_result_; }

 private:
  RobotsRulesDecider::CheckResult check_result_;
  bool did_receive_result_ = false;
  base::WeakPtrFactory<CheckResultReceiver> weak_ptr_factory_{this};
};

std::string GetRobotsRulesProtoString(const std::vector<Rule>& patterns) {
  proto::RobotsRules robots_rules;
  for (const auto& pattern : patterns) {
    auto* new_rule = robots_rules.add_image_ordered_rules();
    if (pattern.rule_type == kRuleTypeAllow) {
      new_rule->set_allowed_pattern(pattern.pattern);
    } else {
      new_rule->set_disallowed_pattern(pattern.pattern);
    }
  }
  return robots_rules.SerializeAsString();
}

class SubresourceRedirectRobotsRulesDeciderTest : public testing::Test {
 public:
  SubresourceRedirectRobotsRulesDeciderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSubresourceRedirect, {{}}}}, {});
  }

  void SetUpRobotsRules(const std::vector<Rule>& patterns) {
    robots_rules_decider_.UpdateRobotsRules(
        GetRobotsRulesProtoString(patterns));
  }

  void CheckRobotsRules(const std::string& url_path_with_query,
                        RobotsRulesDecider::CheckResult expected_result) {
    CheckResultReceiver result_receiver;
    robots_rules_decider_.CheckRobotsRules(
        GURL(kTestOrigin + url_path_with_query), result_receiver.GetCallback());
    EXPECT_TRUE(result_receiver.did_receive_result());
    EXPECT_EQ(expected_result, result_receiver.check_result());
  }

  std::unique_ptr<CheckResultReceiver> CheckRobotsRulesAsync(
      const std::string& url_path_with_query) {
    auto result_receiver = std::make_unique<CheckResultReceiver>();
    robots_rules_decider_.CheckRobotsRules(
        GURL(kTestOrigin + url_path_with_query),
        result_receiver->GetCallback());
    return result_receiver;
  }

  void VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult result) {
    histogram_tester().ExpectUniqueSample(
        "SubresourceRedirect.RobotRulesDecider.ReceiveResult", result, 1);
  }

  void VerifyReceivedRobotsRulesCountHistogram(size_t count) {
    histogram_tester().ExpectUniqueSample(
        "SubresourceRedirect.RobotRulesDecider.Count", count, 1);
  }

  void VerifyTotalRobotsRulesApplyHistograms(size_t total) {
    histogram_tester().ExpectTotalCount(
        "SubresourceRedirect.RobotRulesDecider.ApplyDuration", total);
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  RobotsRulesDecider robots_rules_decider_;
};

TEST_F(SubresourceRedirectRobotsRulesDeciderTest,
       InvalidProtoParseErrorDisallowsAllPaths) {
  robots_rules_decider_.UpdateRobotsRules("INVALID PROTO");
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kParseError);
  histogram_tester().ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.Count", 0);

  // All url paths should be disallowed.
  CheckRobotsRules("", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/foo/bar.jpg",
                   RobotsRulesDecider::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(0);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest, EmptyRulesAllowsAllPaths) {
  SetUpRobotsRules({});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(0);

  // All url paths should be allowed.
  CheckRobotsRules("", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/foo/bar.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  VerifyTotalRobotsRulesApplyHistograms(4);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest,
       RulesReceiveTimeoutDisallowsAllPaths) {
  // Let the rule fetch timeout.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kTimeout);

  // All url paths should be disallowed.
  CheckRobotsRules("", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/foo/bar.jpg",
                   RobotsRulesDecider::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(0);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest,
       CheckResultCallbackAfterRulesReceived) {
  auto receiver1 = CheckRobotsRulesAsync("/foo.jpg");
  auto receiver2 = CheckRobotsRulesAsync("/bar");
  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());
  VerifyTotalRobotsRulesApplyHistograms(0);

  // Once the rules are received the callback should get called with the result.
  SetUpRobotsRules({{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(2);

  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesDecider::CheckResult::kAllowed,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesDecider::CheckResult::kDisallowed,
            receiver2->check_result());
  VerifyTotalRobotsRulesApplyHistograms(2);

  CheckRobotsRules("/foo.png", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/bar", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/baz", RobotsRulesDecider::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(5);
}

// Verify if the callback is called before the decider gets destroyed.
TEST_F(SubresourceRedirectRobotsRulesDeciderTest,
       VerifyCallbackCalledBeforeDeciderDestroy) {
  auto robots_rules_decider = std::make_unique<RobotsRulesDecider>();
  auto receiver1 = std::make_unique<CheckResultReceiver>();
  auto receiver2 = std::make_unique<CheckResultReceiver>();

  robots_rules_decider->CheckRobotsRules(GURL("https://test.com/foo.jpg"),
                                         receiver1->GetCallback());
  robots_rules_decider->CheckRobotsRules(GURL("https://test.com/bar"),
                                         receiver2->GetCallback());
  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());
  VerifyTotalRobotsRulesApplyHistograms(0);

  robots_rules_decider->UpdateRobotsRules(GetRobotsRulesProtoString(
      {{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/"}}));

  // Destroying the decider should trigger the callbacks.
  robots_rules_decider.reset();

  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesDecider::CheckResult::kAllowed,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesDecider::CheckResult::kDisallowed,
            receiver2->check_result());
  VerifyTotalRobotsRulesApplyHistograms(2);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest,
       CheckResultCallbackAfterRulesReceiveTimeout) {
  auto receiver1 = CheckRobotsRulesAsync("/foo.jpg");
  auto receiver2 = CheckRobotsRulesAsync("/bar");
  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());
  VerifyTotalRobotsRulesApplyHistograms(0);

  // Once the rule fetch timesout,the callback should get called with the
  // result.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kTimeout);

  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesDecider::CheckResult::kTimedout,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesDecider::CheckResult::kTimedout,
            receiver2->check_result());
  VerifyTotalRobotsRulesApplyHistograms(0);

  SetUpRobotsRules({{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/"}});

  CheckRobotsRules("/foo.png", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/bar", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/baz", RobotsRulesDecider::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(3);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest, FullAllowRule) {
  SetUpRobotsRules({{kRuleTypeAllow, "*"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(1);

  // All url paths should be allowed.
  CheckRobotsRules("", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/foo/bar.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  VerifyTotalRobotsRulesApplyHistograms(4);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest, FullDisallowRule) {
  SetUpRobotsRules({{kRuleTypeDisallow, "*"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(1);

  // All url paths should be disallowed.
  CheckRobotsRules("", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/foo/bar.jpg",
                   RobotsRulesDecider::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(4);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest, TwoAllowRules) {
  SetUpRobotsRules({{kRuleTypeAllow, "/foo"},
                    {kRuleTypeAllow, "/bar$"},
                    {kRuleTypeDisallow, "*"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(3);

  CheckRobotsRules("", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/foo/baz.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/bar", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/bar.jpg", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/baz", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("foo", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("bar", RobotsRulesDecider::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(9);
}

// When the URL path matches multiple allow and disallow rules, whichever rule
// that comes first in the list should take precedence.
TEST_F(SubresourceRedirectRobotsRulesDeciderTest,
       FirstAllowRuleMatchOverridesLaterDisallowRule) {
  SetUpRobotsRules({{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/foo"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(2);

  CheckRobotsRules("/foo", RobotsRulesDecider::CheckResult::kAllowed);

  SetUpRobotsRules({{kRuleTypeDisallow, "/foo"}, {kRuleTypeAllow, "/foo"}});
  CheckRobotsRules("/foo", RobotsRulesDecider::CheckResult::kDisallowed);

  SetUpRobotsRules({{kRuleTypeAllow, "/foo.jpg"}, {kRuleTypeDisallow, "/foo"}});
  CheckRobotsRules("/foo.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/foo", RobotsRulesDecider::CheckResult::kDisallowed);

  SetUpRobotsRules({{kRuleTypeDisallow, "/foo.jpg"}, {kRuleTypeAllow, "/foo"}});
  CheckRobotsRules("/foo.jpg", RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/foo", RobotsRulesDecider::CheckResult::kAllowed);

  VerifyTotalRobotsRulesApplyHistograms(6);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest, TestURLWithArguments) {
  SetUpRobotsRules({{kRuleTypeAllow, "/*.jpg$"},
                    {kRuleTypeDisallow, "/*.png?*arg_disallowed"},
                    {kRuleTypeAllow, "/*.png"},
                    {kRuleTypeDisallow, "/*.gif?*arg_disallowed"},
                    {kRuleTypeAllow, "/*.gif"},
                    {kRuleTypeDisallow, "/"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesDecider::SubresourceRedirectRobotsRulesReceiveResult::
          kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(6);

  CheckRobotsRules("/allowed.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.png", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.gif", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/disallowed.jpg?arg",
                   RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/allowed.png?arg",
                   RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.png?arg_allowed",
                   RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.png?arg_allowed&arg_allowed2",
                   RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.png?arg_disallowed",
                   RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/allowed.png?arg_disallowed&arg_disallowed2",
                   RobotsRulesDecider::CheckResult::kDisallowed);

  VerifyTotalRobotsRulesApplyHistograms(9);
}

TEST_F(SubresourceRedirectRobotsRulesDeciderTest, TestRulesAreCaseSensitive) {
  SetUpRobotsRules({{kRuleTypeAllow, "/allowed"},
                    {kRuleTypeAllow, "/CamelCase"},
                    {kRuleTypeAllow, "/CAPITALIZE"},
                    {kRuleTypeDisallow, "/"}});
  VerifyReceivedRobotsRulesCountHistogram(4);

  CheckRobotsRules("/allowed.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/CamelCase.jpg", RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/CAPITALIZE.jpg",
                   RobotsRulesDecider::CheckResult::kAllowed);
  CheckRobotsRules("/Allowed.jpg",
                   RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/camelcase.jpg",
                   RobotsRulesDecider::CheckResult::kDisallowed);
  CheckRobotsRules("/capitalize.jpg",
                   RobotsRulesDecider::CheckResult::kDisallowed);

  VerifyTotalRobotsRulesApplyHistograms(6);
}

}  // namespace

}  // namespace subresource_redirect
