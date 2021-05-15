// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/subresource_redirect_service.mojom.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser_cache.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_redirect/proto/robots_rules.pb.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace subresource_redirect {

constexpr char kTestOrigin[] = "https://test.com";

const int kRenderFrameID = 1;

constexpr base::TimeDelta kFetchTimeout = base::TimeDelta::FromSeconds(1);

// Provides a callback to send to robots rules parser, and to check the state of
// the callback and its result.
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

class SubresourceRedirectRobotsRulesParserCacheTest
    : public ChromeRenderViewTest {
 public:
  // Verify robots rules check result is received synchronously with the
  // expected result.
  void CheckRobotsRules(const GURL& url,
                        RobotsRulesParser::CheckResult expected_result,
                        int render_frame_id = kRenderFrameID) {
    CheckResultReceiver result_receiver;
    auto result = robots_rules_parser_cache_.CheckRobotsRules(
        render_frame_id, url, result_receiver.GetCallback());
    EXPECT_FALSE(result_receiver.did_receive_result());
    EXPECT_EQ(expected_result, result);
  }

  // Verify robots rules check result is received asynchronously, and returns
  // the receiver that can be used to check the result.
  std::unique_ptr<CheckResultReceiver> CheckRobotsRulesAsync(
      const GURL& url,
      int render_frame_id = kRenderFrameID) {
    auto result_receiver = std::make_unique<CheckResultReceiver>();
    EXPECT_FALSE(robots_rules_parser_cache_.CheckRobotsRules(
        render_frame_id, url, result_receiver->GetCallback()));
    return result_receiver;
  }

 protected:
  base::HistogramTester histogram_tester_;
  RobotsRulesParserCache robots_rules_parser_cache_;
};

TEST_F(SubresourceRedirectRobotsRulesParserCacheTest, AllowAndDisallowRules) {
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  EXPECT_FALSE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));
  robots_rules_parser_cache_.CreateRobotsRulesParser(origin, kFetchTimeout);
  EXPECT_TRUE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));
  robots_rules_parser_cache_.UpdateRobotsRules(
      origin, GetRobotsRulesProtoString(
                  {{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/bar"}}));

  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/foo.jpg"})),
                   RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/bar.jpg"})),
                   RobotsRulesParser::CheckResult::kDisallowed);
  CheckRobotsRules(GURL(kTestOrigin), RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/"})),
                   RobotsRulesParser::CheckResult::kAllowed);
}

TEST_F(SubresourceRedirectRobotsRulesParserCacheTest,
       AsyncAllowAndDisallowRules) {
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  EXPECT_FALSE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));
  robots_rules_parser_cache_.CreateRobotsRulesParser(origin, kFetchTimeout);
  EXPECT_TRUE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));

  // Async check for robots rules.
  auto receiver1 =
      CheckRobotsRulesAsync(GURL(base::StrCat({kTestOrigin, "/foo.jpg"})));
  auto receiver2 =
      CheckRobotsRulesAsync(GURL(base::StrCat({kTestOrigin, "/bar.jpg"})));
  auto receiver3 = CheckRobotsRulesAsync(GURL(kTestOrigin));
  auto receiver4 =
      CheckRobotsRulesAsync(GURL(base::StrCat({kTestOrigin, "/"})));
  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());
  EXPECT_FALSE(receiver3->did_receive_result());
  EXPECT_FALSE(receiver4->did_receive_result());

  // Send the rules now.
  robots_rules_parser_cache_.UpdateRobotsRules(
      origin, GetRobotsRulesProtoString(
                  {{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/bar"}}));

  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_TRUE(receiver3->did_receive_result());
  EXPECT_TRUE(receiver4->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kAllowed,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kDisallowed,
            receiver2->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kAllowed,
            receiver3->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kAllowed,
            receiver4->check_result());

  // Sync check for robots rules
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/foo.jpg"})),
                   RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/bar.jpg"})),
                   RobotsRulesParser::CheckResult::kDisallowed);
}

TEST_F(SubresourceRedirectRobotsRulesParserCacheTest, RulesFetchTimeout) {
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  EXPECT_FALSE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));
  robots_rules_parser_cache_.CreateRobotsRulesParser(origin, kFetchTimeout);
  EXPECT_TRUE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));

  // Async check for robots rules.
  auto receiver1 =
      CheckRobotsRulesAsync(GURL(base::StrCat({kTestOrigin, "/foo.jpg"})));
  auto receiver2 =
      CheckRobotsRulesAsync(GURL(base::StrCat({kTestOrigin, "/bar.jpg"})));
  auto receiver3 = CheckRobotsRulesAsync(GURL(kTestOrigin));
  auto receiver4 =
      CheckRobotsRulesAsync(GURL(base::StrCat({kTestOrigin, "/"})));
  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());
  EXPECT_FALSE(receiver3->did_receive_result());
  EXPECT_FALSE(receiver4->did_receive_result());

  // Let rule fetch timeout.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_TRUE(receiver3->did_receive_result());
  EXPECT_TRUE(receiver4->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kTimedout,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kTimedout,
            receiver2->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kTimedout,
            receiver3->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kTimedout,
            receiver4->check_result());

  // Sync check for robots rules
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/foo.jpg"})),
                   RobotsRulesParser::CheckResult::kDisallowedAfterTimeout);
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/bar.jpg"})),
                   RobotsRulesParser::CheckResult::kDisallowedAfterTimeout);
}

TEST_F(SubresourceRedirectRobotsRulesParserCacheTest,
       InvalidatePendingRequests) {
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  EXPECT_FALSE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));
  robots_rules_parser_cache_.CreateRobotsRulesParser(origin, kFetchTimeout);
  EXPECT_TRUE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));

  auto receiver1 = CheckRobotsRulesAsync(
      GURL(base::StrCat({kTestOrigin, "/foo.jpg"})), 1 /* routing_id */);
  auto receiver2 = CheckRobotsRulesAsync(
      GURL(base::StrCat({kTestOrigin, "/bar.jpg"})), 2 /* routing_id */);

  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());

  robots_rules_parser_cache_.InvalidatePendingRequests(1);
  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kInvalidated,
            receiver1->check_result());

  robots_rules_parser_cache_.InvalidatePendingRequests(2);
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kInvalidated,
            receiver2->check_result());
}

TEST_F(SubresourceRedirectRobotsRulesParserCacheTest,
       CheckRulesBeforeCreating) {
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  EXPECT_FALSE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));

  // Check for rules before creating the parser.
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/foo.jpg"})),
                   RobotsRulesParser::CheckResult::kEntryMissing);

  // UpdateRobotsRules should have no effect.
  robots_rules_parser_cache_.UpdateRobotsRules(
      origin, GetRobotsRulesProtoString(
                  {{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/bar"}}));
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/foo.jpg"})),
                   RobotsRulesParser::CheckResult::kEntryMissing);
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/bar.jpg"})),
                   RobotsRulesParser::CheckResult::kEntryMissing);

  // Now, create the parser.
  robots_rules_parser_cache_.CreateRobotsRulesParser(origin, kFetchTimeout);
  EXPECT_TRUE(robots_rules_parser_cache_.DoRobotsRulesParserExist(origin));

  auto receiver1 = CheckRobotsRulesAsync(
      GURL(base::StrCat({kTestOrigin, "/foo.jpg"})), 1 /* routing_id */);
  auto receiver2 = CheckRobotsRulesAsync(
      GURL(base::StrCat({kTestOrigin, "/bar.jpg"})), 2 /* routing_id */);

  EXPECT_FALSE(receiver1->did_receive_result());
  EXPECT_FALSE(receiver2->did_receive_result());

  // Update robots rules now.
  robots_rules_parser_cache_.UpdateRobotsRules(
      origin, GetRobotsRulesProtoString(
                  {{kRuleTypeAllow, "/foo"}, {kRuleTypeDisallow, "/bar"}}));
  EXPECT_TRUE(receiver1->did_receive_result());
  EXPECT_TRUE(receiver2->did_receive_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kAllowed,
            receiver1->check_result());
  EXPECT_EQ(RobotsRulesParser::CheckResult::kDisallowed,
            receiver2->check_result());

  // Sync check for robots rules
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/foo.jpg"})),
                   RobotsRulesParser::CheckResult::kAllowed);
  CheckRobotsRules(GURL(base::StrCat({kTestOrigin, "/bar.jpg"})),
                   RobotsRulesParser::CheckResult::kDisallowed);
}

}  // namespace subresource_redirect
