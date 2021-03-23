// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"
#include "components/subresource_redirect/proto/robots_rules.pb.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace subresource_redirect {

constexpr char kTestOrigin[] = "https://test.com";

class CheckResultReceiver {
 public:
  void OnCheckRobotsRulesResult(RobotsRulesParser::CheckResult check_result) {
    EXPECT_FALSE(did_receive_result_);
    did_receive_result_ = true;
    check_result_ = check_result;
  }
  RobotsRulesParser::CheckResultCallback GetCallback() {
    return base::BindOnce(&CheckResultReceiver::OnCheckRobotsRulesResult,
                          weak_ptr_factory_.GetWeakPtr());
  }
  RobotsRulesParser::CheckResult check_result() const { return check_result_; }
  bool did_receive_result() const { return did_receive_result_; }

 private:
  RobotsRulesParser::CheckResult check_result_;
  bool did_receive_result_ = false;
  base::WeakPtrFactory<CheckResultReceiver> weak_ptr_factory_{this};
};

const int kRenderFrameID = 1;

class SubresourceRedirectRobotsRulesParserTest : public testing::Test {
 public:
  SubresourceRedirectRobotsRulesParserTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        robots_rules_parser_(base::TimeDelta::FromSeconds(1)) {}
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSubresourceRedirect, {{}}}}, {});
  }

  void SetUpRobotsRules(const std::vector<RobotsRule>& patterns) {
    robots_rules_parser_.UpdateRobotsRules(GetRobotsRulesProtoString(patterns));
    VerifyRulesReceiveState(RobotsRulesParser::RulesReceiveState::kSuccess);
  }

  // Verify robots rules check result is received synchronously with the
  // expected result.
  void CheckRobotsRules(const std::string& url_path_with_query,
                        RobotsRulesParser::CheckResult expected_result,
                        int render_frame_id = kRenderFrameID) {
    CheckResultReceiver result_receiver;
    auto result = robots_rules_parser_.CheckRobotsRules(
        render_frame_id, GURL(kTestOrigin + url_path_with_query),
        result_receiver.GetCallback());
    EXPECT_FALSE(result_receiver.did_receive_result());
    EXPECT_EQ(expected_result, result);
  }

  // Verify robots rules check result is received asynchronously, and returns
  // the receiver that can be used to check the result.
  std::unique_ptr<CheckResultReceiver> CheckRobotsRulesAsync(
      const std::string& url_path_with_query,
      int render_frame_id = kRenderFrameID) {
    auto result_receiver = std::make_unique<CheckResultReceiver>();
    EXPECT_FALSE(robots_rules_parser_.CheckRobotsRules(
        render_frame_id, GURL(kTestOrigin + url_path_with_query),
        result_receiver->GetCallback()));
    return result_receiver;
  }

  void VerifyRulesReceiveState(
      RobotsRulesParser::RulesReceiveState expected_rules_receive_state) {
    EXPECT_EQ(robots_rules_parser_.rules_receive_state_,
              expected_rules_receive_state);
  }

  void VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult result) {
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
  RobotsRulesParser robots_rules_parser_;
};

TEST_F(SubresourceRedirectRobotsRulesParserTest,
       InvalidProtoParseErrorDisallowsAllPaths) {
  VerifyRulesReceiveState(RobotsRulesParser::RulesReceiveState::kTimerRunning);
  robots_rules_parser_.UpdateRobotsRules("INVALID PROTO");
  VerifyRulesReceiveState(RobotsRulesParser::RulesReceiveState::kParseFailed);
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::
          kParseError);
  histogram_tester().ExpectTotalCount(
      "SubresourceRedirect.RobotRulesDecider.Count", 0);

  // All url paths should be disallowed.
  CheckRobotsRules("", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/foo/bar.jpg", RobotsRulesParser::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(0);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest, EmptyRulesAllowsAllPaths) {
  VerifyRulesReceiveState(RobotsRulesParser::RulesReceiveState::kTimerRunning);
  SetUpRobotsRules({});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(0);

  // All url paths should be allowed.
  CheckRobotsRules("", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/foo/bar.jpg", RobotsRulesParser::CheckResult::kAllowed);
  VerifyTotalRobotsRulesApplyHistograms(4);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest,
       RulesReceiveTimeoutDisallowsAllPaths) {
  // Let the rule fetch timeout.
  VerifyRulesReceiveState(RobotsRulesParser::RulesReceiveState::kTimerRunning);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kTimeout);
  VerifyRulesReceiveState(RobotsRulesParser::RulesReceiveState::kTimeout);

  // All url paths should be disallowed due to timeout.
  CheckRobotsRules("", RobotsRulesParser::CheckResult::kDisallowedAfterTimeout);
  CheckRobotsRules("/",
                   RobotsRulesParser::CheckResult::kDisallowedAfterTimeout);
  CheckRobotsRules("/foo.jpg",
                   RobotsRulesParser::CheckResult::kDisallowedAfterTimeout);
  CheckRobotsRules("/foo/bar.jpg",
                   RobotsRulesParser::CheckResult::kDisallowedAfterTimeout);
  VerifyTotalRobotsRulesApplyHistograms(0);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest,
       CheckResultCallbackAfterRulesReceived) {
  auto receiver1 = CheckRobotsRulesAsync("/foo.jpg");
  auto receiver2 = CheckRobotsRulesAsync("/bar");
  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());
  VerifyTotalRobotsRulesApplyHistograms(0);

  // Once the rules are received the callback should get called with the result.
  VerifyRulesReceiveState(RobotsRulesParser::RulesReceiveState::kTimerRunning);
  SetUpRobotsRules({{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(2);

  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kAllowed,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kDisallowed,
            receiver2->check_result());
  VerifyTotalRobotsRulesApplyHistograms(2);

  CheckRobotsRules("/foo.png", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/bar", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/baz", RobotsRulesParser::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(5);
}

// Verify if the callback is called before the decider gets destroyed.
TEST_F(SubresourceRedirectRobotsRulesParserTest,
       VerifyCallbackCalledBeforeDeciderDestroy) {
  auto robots_rules_parser =
      std::make_unique<RobotsRulesParser>(base::TimeDelta::FromSeconds(1));
  auto receiver1 = std::make_unique<CheckResultReceiver>();
  auto receiver2 = std::make_unique<CheckResultReceiver>();

  robots_rules_parser->CheckRobotsRules(kRenderFrameID,
                                        GURL("https://test.com/foo.jpg"),
                                        receiver1->GetCallback());
  robots_rules_parser->CheckRobotsRules(
      kRenderFrameID, GURL("https://test.com/bar"), receiver2->GetCallback());
  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());
  VerifyTotalRobotsRulesApplyHistograms(0);

  robots_rules_parser->UpdateRobotsRules(GetRobotsRulesProtoString(
      {{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/"}}));

  // Destroying the decider should trigger the callbacks.
  robots_rules_parser.reset();

  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kAllowed,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kDisallowed,
            receiver2->check_result());
  VerifyTotalRobotsRulesApplyHistograms(2);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest,
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
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kTimeout);
  VerifyRulesReceiveState(RobotsRulesParser::RulesReceiveState::kTimeout);

  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kTimedout,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kTimedout,
            receiver2->check_result());
  VerifyTotalRobotsRulesApplyHistograms(0);

  SetUpRobotsRules({{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/"}});

  CheckRobotsRules("/foo.png", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/bar", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/baz", RobotsRulesParser::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(3);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest, FullAllowRule) {
  SetUpRobotsRules({{kRuleTypeAllow, "*"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(1);

  // All url paths should be allowed.
  CheckRobotsRules("", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/foo/bar.jpg", RobotsRulesParser::CheckResult::kAllowed);
  VerifyTotalRobotsRulesApplyHistograms(4);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest, FullDisallowRule) {
  SetUpRobotsRules({{kRuleTypeDisallow, "*"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(1);

  // All url paths should be disallowed.
  CheckRobotsRules("", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/foo/bar.jpg", RobotsRulesParser::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(4);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest, TwoAllowRules) {
  SetUpRobotsRules({{kRuleTypeAllow, "/foo"},
                    {kRuleTypeAllow, "/bar$"},
                    {kRuleTypeDisallow, "*"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(3);

  CheckRobotsRules("", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/foo.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/foo/baz.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/bar", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/bar.jpg", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/baz", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("foo", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("bar", RobotsRulesParser::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(9);
}

// When the URL path matches multiple allow and disallow rules, whichever rule
// that comes first in the list should take precedence.
TEST_F(SubresourceRedirectRobotsRulesParserTest,
       FirstAllowRuleMatchOverridesLaterDisallowRule) {
  SetUpRobotsRules({{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/foo"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(2);

  CheckRobotsRules("/foo", RobotsRulesParser::CheckResult::kAllowed);

  SetUpRobotsRules({{kRuleTypeDisallow, "/foo"}, {kRuleTypeAllow, "/foo"}});
  CheckRobotsRules("/foo", RobotsRulesParser::CheckResult::kDisallowed);

  SetUpRobotsRules({{kRuleTypeAllow, "/foo.jpg"}, {kRuleTypeDisallow, "/foo"}});
  CheckRobotsRules("/foo.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/foo", RobotsRulesParser::CheckResult::kDisallowed);

  SetUpRobotsRules({{kRuleTypeDisallow, "/foo.jpg"}, {kRuleTypeAllow, "/foo"}});
  CheckRobotsRules("/foo.jpg", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/foo", RobotsRulesParser::CheckResult::kAllowed);

  VerifyTotalRobotsRulesApplyHistograms(6);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest, TestURLWithArguments) {
  SetUpRobotsRules({{kRuleTypeAllow, "/*.jpg$"},
                    {kRuleTypeDisallow, "/*.png?*arg_disallowed"},
                    {kRuleTypeAllow, "/*.png"},
                    {kRuleTypeDisallow, "/*.gif?*arg_disallowed"},
                    {kRuleTypeAllow, "/*.gif"},
                    {kRuleTypeDisallow, "/"}});
  VerifyRobotsRulesReceiveResultHistogram(
      RobotsRulesParser::SubresourceRedirectRobotsRulesReceiveResult::kSuccess);
  VerifyReceivedRobotsRulesCountHistogram(6);

  CheckRobotsRules("/allowed.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.png", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.gif", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/disallowed.jpg?arg",
                   RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/allowed.png?arg",
                   RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.png?arg_allowed",
                   RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.png?arg_allowed&arg_allowed2",
                   RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/allowed.png?arg_disallowed",
                   RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/allowed.png?arg_disallowed&arg_disallowed2",
                   RobotsRulesParser::CheckResult::kDisallowed);

  VerifyTotalRobotsRulesApplyHistograms(9);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest, TestRulesAreCaseSensitive) {
  SetUpRobotsRules({{kRuleTypeAllow, "/allowed"},
                    {kRuleTypeAllow, "/CamelCase"},
                    {kRuleTypeAllow, "/CAPITALIZE"},
                    {kRuleTypeDisallow, "/"}});
  VerifyReceivedRobotsRulesCountHistogram(4);

  CheckRobotsRules("/allowed.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/CamelCase.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/CAPITALIZE.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/Allowed.jpg", RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/camelcase.jpg",
                   RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules("/capitalize.jpg",
                   RobotsRulesParser::CheckResult::kDisallowed);

  VerifyTotalRobotsRulesApplyHistograms(6);
}

TEST_F(SubresourceRedirectRobotsRulesParserTest,
       TestInvalidatePendingRequests) {
  auto receiver1 = CheckRobotsRulesAsync("/allowed.jpg", 1 /*render_frame_id*/);
  auto receiver2 =
      CheckRobotsRulesAsync("/disallowed.jpg", 1 /*render_frame_id*/);
  auto receiver3 = CheckRobotsRulesAsync("/allowed.jpg", 2 /*render_frame_id*/);
  auto receiver4 =
      CheckRobotsRulesAsync("/disallowed.jpg", 2 /*render_frame_id*/);

  // Invalidate should cancel requests for that render frame ID.
  robots_rules_parser_.InvalidatePendingRequests(1 /*render_frame_id*/);
  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kInvalidated,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kInvalidated,
            receiver2->check_result());
  EXPECT_FALSE(receiver3->did_receive_result());
  EXPECT_FALSE(receiver4->did_receive_result());
  VerifyTotalRobotsRulesApplyHistograms(0);

  // When robots rules are retrieved, the
  SetUpRobotsRules({{kRuleTypeAllow, "/allow*"}, {kRuleTypeDisallow, "/*"}});
  EXPECT_TRUE(receiver3->did_receive_result());
  EXPECT_TRUE(receiver4->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kAllowed,
            receiver3->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kDisallowed,
            receiver4->check_result());
  VerifyTotalRobotsRulesApplyHistograms(2);

  CheckRobotsRules("/allowed.jpg", RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules("/disallowed.jpg",
                   RobotsRulesParser::CheckResult::kDisallowed);
  VerifyTotalRobotsRulesApplyHistograms(4);
}

}  // namespace subresource_redirect
