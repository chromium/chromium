// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/core/eligibility/eligibility_decider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_distiller {

namespace {

using optimization_guide::OptimizationGuideDecision;

constexpr base::TimeDelta kStartDelay = base::Seconds(1);

struct Result {
  GURL url;
  std::optional<ReaderModeEligibility> result;
  bool is_current_url;
};

class FakeDelegate : public EligibilityDecider::Delegate {
 public:
  // EligibilityDecider::Delegate:
  GURL GetLastCommittedURL() const override { return last_committed_url; }
  bool IsContentHTML() const override { return is_content_html; }
  void ExecuteScript(
      const std::string& script,
      base::OnceCallback<void(const base::Value*)> callback) override {
    ++script_run_count;
    if (!has_main_frame) {
      return;
    }
    last_script = script;
    script_callback = std::move(callback);
  }

  void RunScriptCallback(const base::Value* value) {
    ASSERT_TRUE(script_callback);
    std::move(script_callback).Run(value);
  }

  GURL last_committed_url;
  bool is_content_html = true;
  bool has_main_frame = true;
  int script_run_count = 0;
  std::string last_script;
  base::OnceCallback<void(const base::Value*)> script_callback;
};

class EligibilityDeciderTest : public testing::Test {
 protected:
  void CreateDecider(
      optimization_guide::OptimizationGuideDecider* optimization_guide) {
    decider_ = std::make_unique<EligibilityDecider>(
        delegate_, optimization_guide, kStartDelay,
        base::BindRepeating(&EligibilityDeciderTest::OnResult,
                            base::Unretained(this)));
  }

  void OnResult(const GURL& url,
                std::optional<ReaderModeEligibility> result,
                bool is_current_url) {
    results_.push_back({url, result, is_current_url});
  }

  // Starts a decision for `url`, which is also the committed URL, and waits
  // for the start delay to elapse.
  void StartAndWait(const GURL& url) {
    delegate_.last_committed_url = url;
    decider_->Start(url);
    task_environment_.FastForwardBy(kStartDelay);
  }

  void ExpectSingleResult(const GURL& url,
                          std::optional<ReaderModeEligibility> result,
                          bool is_current_url) {
    ASSERT_EQ(1u, results_.size());
    EXPECT_EQ(url, results_[0].url);
    EXPECT_EQ(result, results_[0].result);
    EXPECT_EQ(is_current_url, results_[0].is_current_url);
  }

  void ExpectOptimizationGuideDecision(const GURL& url,
                                       OptimizationGuideDecision decision) {
    EXPECT_CALL(
        optimization_guide_,
        CanApplyOptimization(
            url, optimization_guide::proto::READER_MODE_ELIGIBLE,
            testing::An<
                optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            decision, optimization_guide::OptimizationMetadata()));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeDelegate delegate_;
  testing::StrictMock<optimization_guide::MockOptimizationGuideDecider>
      optimization_guide_;
  std::unique_ptr<EligibilityDecider> decider_;
  std::vector<Result> results_;
  const GURL article_url_{"https://example.com/article"};
  const base::Value true_value_{true};
  const base::Value false_value_{false};
};

// Tests that script values map to eligible, not eligible, or malformed.
TEST(ReaderModeEligibilityTest, FromScriptValue) {
  base::Value true_value(true);
  base::Value false_value(false);
  base::Value int_value(1);
  EXPECT_EQ(ReaderModeEligibility::kReaderModeEligible,
            EligibilityFromScriptValue(&true_value));
  EXPECT_EQ(ReaderModeEligibility::kReaderModeNotEligibleContentAndLength,
            EligibilityFromScriptValue(&false_value));
  EXPECT_EQ(ReaderModeEligibility::kMalformedResponse,
            EligibilityFromScriptValue(&int_value));
  EXPECT_EQ(ReaderModeEligibility::kMalformedResponse,
            EligibilityFromScriptValue(nullptr));
}

// Tests that each Optimization Guide decision maps to its eligibility value.
TEST(ReaderModeEligibilityTest, FromOptimizationGuideDecision) {
  EXPECT_EQ(ReaderModeEligibility::kReaderModeEligible,
            EligibilityFromOptimizationGuideDecision(
                OptimizationGuideDecision::kTrue));
  EXPECT_EQ(
      ReaderModeEligibility::kReaderModeNotEligibleOptimizationGuideIneligible,
      EligibilityFromOptimizationGuideDecision(
          OptimizationGuideDecision::kFalse));
  EXPECT_EQ(
      ReaderModeEligibility::kReaderModeNotEligibleOptimizationGuideUnknown,
      EligibilityFromOptimizationGuideDecision(
          OptimizationGuideDecision::kUnknown));
}

// Tests that non-HTTP(S), Google home/search, YouTube, and Workspace URLs are
// rejected.
TEST(EligibilityDeciderUrlGateTest, IsUrlEligible) {
  EXPECT_TRUE(IsUrlEligible(GURL("https://example.com/article")));
  EXPECT_TRUE(IsUrlEligible(GURL("http://example.com/article")));
  EXPECT_FALSE(IsUrlEligible(GURL("ftp://example.com/article")));
  EXPECT_FALSE(IsUrlEligible(GURL("chrome://flags")));
  EXPECT_FALSE(IsUrlEligible(GURL("https://www.google.com/")));
  EXPECT_FALSE(IsUrlEligible(GURL("https://www.google.com/search?q=a")));
  EXPECT_FALSE(IsUrlEligible(GURL("https://m.youtube.com/watch?v=a")));
  EXPECT_FALSE(IsUrlEligible(GURL("https://docs.google.com/d/a")));
  EXPECT_FALSE(IsUrlEligible(GURL("https://mail.google.com/")));
}

// Tests that the script does not run until the start delay has elapsed.
TEST_F(EligibilityDeciderTest, WaitsForStartDelay) {
  CreateDecider(&optimization_guide_);
  delegate_.last_committed_url = article_url_;
  decider_->Start(article_url_);

  task_environment_.FastForwardBy(kStartDelay - base::Milliseconds(1));
  EXPECT_EQ(0, delegate_.script_run_count);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(1, delegate_.script_run_count);
  EXPECT_FALSE(delegate_.last_script.empty());
}

// Tests that an ineligible URL reports a null result without running the
// script.
TEST_F(EligibilityDeciderTest, UrlGateRejectsWithoutRunningScript) {
  CreateDecider(&optimization_guide_);
  GURL search_url("https://www.google.com/search?q=a");

  StartAndWait(search_url);

  EXPECT_EQ(0, delegate_.script_run_count);
  ExpectSingleResult(search_url, std::nullopt, /*is_current_url=*/true);
}

// Tests that non-HTML content reports a null result without running the
// script.
TEST_F(EligibilityDeciderTest, NonHtmlRejectsWithoutRunningScript) {
  CreateDecider(&optimization_guide_);
  delegate_.is_content_html = false;

  StartAndWait(article_url_);

  EXPECT_EQ(0, delegate_.script_run_count);
  ExpectSingleResult(article_url_, std::nullopt, /*is_current_url=*/true);
}

// Tests that a non-boolean script result is reported as malformed.
TEST_F(EligibilityDeciderTest, MalformedScriptResult) {
  CreateDecider(&optimization_guide_);
  StartAndWait(article_url_);

  base::Value string_value("true");
  delegate_.RunScriptCallback(&string_value);

  ExpectSingleResult(article_url_, ReaderModeEligibility::kMalformedResponse,
                     /*is_current_url=*/true);
}

// Tests that a false script result is reported without querying the
// Optimization Guide.
TEST_F(EligibilityDeciderTest, ScriptFalseSkipsOptimizationGuide) {
  CreateDecider(&optimization_guide_);
  StartAndWait(article_url_);

  delegate_.RunScriptCallback(&false_value_);

  ExpectSingleResult(
      article_url_,
      ReaderModeEligibility::kReaderModeNotEligibleContentAndLength,
      /*is_current_url=*/true);
}

// Tests that a true Optimization Guide decision reports the page eligible.
TEST_F(EligibilityDeciderTest, OptimizationGuideTrue) {
  CreateDecider(&optimization_guide_);
  ExpectOptimizationGuideDecision(article_url_,
                                  OptimizationGuideDecision::kTrue);
  StartAndWait(article_url_);

  delegate_.RunScriptCallback(&true_value_);

  ExpectSingleResult(article_url_, ReaderModeEligibility::kReaderModeEligible,
                     /*is_current_url=*/true);
}

// Tests that a false Optimization Guide decision reports the page ineligible.
TEST_F(EligibilityDeciderTest, OptimizationGuideFalse) {
  CreateDecider(&optimization_guide_);
  ExpectOptimizationGuideDecision(article_url_,
                                  OptimizationGuideDecision::kFalse);
  StartAndWait(article_url_);

  delegate_.RunScriptCallback(&true_value_);

  ExpectSingleResult(
      article_url_,
      ReaderModeEligibility::kReaderModeNotEligibleOptimizationGuideIneligible,
      /*is_current_url=*/true);
}

// Tests that an unknown Optimization Guide decision reports the page
// ineligible (unknown).
TEST_F(EligibilityDeciderTest, OptimizationGuideUnknown) {
  CreateDecider(&optimization_guide_);
  ExpectOptimizationGuideDecision(article_url_,
                                  OptimizationGuideDecision::kUnknown);
  StartAndWait(article_url_);

  delegate_.RunScriptCallback(&true_value_);

  ExpectSingleResult(
      article_url_,
      ReaderModeEligibility::kReaderModeNotEligibleOptimizationGuideUnknown,
      /*is_current_url=*/true);
}

// Tests that the script result is reported directly when there is no
// Optimization Guide.
TEST_F(EligibilityDeciderTest, NullOptimizationGuideUsesScriptResult) {
  CreateDecider(/*optimization_guide=*/nullptr);
  StartAndWait(article_url_);

  delegate_.RunScriptCallback(&true_value_);

  ExpectSingleResult(article_url_, ReaderModeEligibility::kReaderModeEligible,
                     /*is_current_url=*/true);
}

// Tests that a URL change during the start delay skips the script and drops
// the decision.
TEST_F(EligibilityDeciderTest, UrlChangeBeforeStartDelaySkipsScript) {
  CreateDecider(&optimization_guide_);
  delegate_.last_committed_url = article_url_;
  decider_->Start(article_url_);

  delegate_.last_committed_url = GURL("https://example.com/other");
  task_environment_.FastForwardBy(kStartDelay);

  EXPECT_EQ(0, delegate_.script_run_count);
  EXPECT_TRUE(results_.empty());
}

// Tests that a URL change while the script runs skips the Optimization Guide
// and drops the decision.
TEST_F(EligibilityDeciderTest,
       UrlChangeDuringScriptSkipsOptimizationGuideAndDropsDecision) {
  CreateDecider(&optimization_guide_);
  StartAndWait(article_url_);

  delegate_.last_committed_url = GURL("https://example.com/other");
  delegate_.RunScriptCallback(&true_value_);

  EXPECT_TRUE(results_.empty());
}

// Tests that, without an Optimization Guide, a URL change while the script
// runs reports the result as not current.
TEST_F(EligibilityDeciderTest,
       StaleUrlWithoutOptimizationGuideReportsNotCurrent) {
  CreateDecider(/*optimization_guide=*/nullptr);
  StartAndWait(article_url_);

  delegate_.last_committed_url = GURL("https://example.com/other");
  delegate_.RunScriptCallback(&true_value_);

  ExpectSingleResult(article_url_, ReaderModeEligibility::kReaderModeEligible,
                     /*is_current_url=*/false);
}

// Tests that a URL change while the Optimization Guide runs reports the
// result as not current.
TEST_F(EligibilityDeciderTest,
       StaleUrlDuringOptimizationGuideReportsNotCurrent) {
  CreateDecider(&optimization_guide_);
  optimization_guide::OptimizationGuideDecisionCallback og_callback;
  EXPECT_CALL(
      optimization_guide_,
      CanApplyOptimization(
          article_url_, optimization_guide::proto::READER_MODE_ELIGIBLE,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&og_callback));
  StartAndWait(article_url_);
  delegate_.RunScriptCallback(&true_value_);
  ASSERT_TRUE(og_callback);

  delegate_.last_committed_url = GURL("https://example.com/other");
  std::move(og_callback)
      .Run(OptimizationGuideDecision::kTrue,
           optimization_guide::OptimizationMetadata());

  ExpectSingleResult(article_url_, ReaderModeEligibility::kReaderModeEligible,
                     /*is_current_url=*/false);
}

// Tests that a fragment-only URL change still counts as the current URL.
TEST_F(EligibilityDeciderTest, RefChangeIsStillCurrent) {
  CreateDecider(/*optimization_guide=*/nullptr);
  delegate_.last_committed_url = article_url_;
  decider_->Start(article_url_);

  delegate_.last_committed_url = GURL("https://example.com/article#section");
  task_environment_.FastForwardBy(kStartDelay);
  delegate_.RunScriptCallback(&true_value_);

  ExpectSingleResult(article_url_, ReaderModeEligibility::kReaderModeEligible,
                     /*is_current_url=*/true);
}

// Tests that restarting replaces the pending timer so only the new URL is
// evaluated.
TEST_F(EligibilityDeciderTest, RestartCancelsPendingTimer) {
  CreateDecider(&optimization_guide_);
  GURL other_url("https://example.com/other");
  delegate_.last_committed_url = article_url_;
  decider_->Start(article_url_);
  delegate_.last_committed_url = other_url;
  decider_->Start(other_url);

  task_environment_.FastForwardBy(kStartDelay);

  EXPECT_EQ(1, delegate_.script_run_count);
  delegate_.RunScriptCallback(&false_value_);
  ExpectSingleResult(
      other_url, ReaderModeEligibility::kReaderModeNotEligibleContentAndLength,
      /*is_current_url=*/true);
}

// Tests that Cancel() returns true only while the start timer is pending and
// stops the decision.
TEST_F(EligibilityDeciderTest, CancelReturnsWhetherDecisionWasPending) {
  CreateDecider(&optimization_guide_);
  EXPECT_FALSE(decider_->Cancel());

  decider_->Start(article_url_);
  EXPECT_TRUE(decider_->Cancel());

  task_environment_.FastForwardBy(kStartDelay);
  EXPECT_EQ(0, delegate_.script_run_count);
  EXPECT_TRUE(results_.empty());
}

// Tests that canceling while the script runs skips the Optimization Guide and
// drops the decision.
TEST_F(EligibilityDeciderTest,
       CancelDuringScriptSkipsOptimizationGuideAndDropsDecision) {
  CreateDecider(&optimization_guide_);
  StartAndWait(article_url_);

  EXPECT_FALSE(decider_->Cancel());
  delegate_.RunScriptCallback(&true_value_);

  EXPECT_TRUE(results_.empty());
}

// Tests that, without an Optimization Guide, canceling while the script runs
// reports the result as not current.
TEST_F(EligibilityDeciderTest,
       CancelDuringScriptWithoutOptimizationGuideReportsNotCurrent) {
  CreateDecider(/*optimization_guide=*/nullptr);
  StartAndWait(article_url_);

  EXPECT_FALSE(decider_->Cancel());
  delegate_.RunScriptCallback(&true_value_);

  ExpectSingleResult(article_url_, ReaderModeEligibility::kReaderModeEligible,
                     /*is_current_url=*/false);
}

// Tests that canceling while the Optimization Guide runs reports the result
// as not current.
TEST_F(EligibilityDeciderTest, CancelDuringOptimizationGuideReportsNotCurrent) {
  CreateDecider(&optimization_guide_);
  optimization_guide::OptimizationGuideDecisionCallback og_callback;
  EXPECT_CALL(
      optimization_guide_,
      CanApplyOptimization(
          article_url_, optimization_guide::proto::READER_MODE_ELIGIBLE,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&og_callback));
  StartAndWait(article_url_);
  delegate_.RunScriptCallback(&true_value_);
  ASSERT_TRUE(og_callback);

  EXPECT_FALSE(decider_->Cancel());
  std::move(og_callback)
      .Run(OptimizationGuideDecision::kTrue,
           optimization_guide::OptimizationMetadata());

  ExpectSingleResult(article_url_, ReaderModeEligibility::kReaderModeEligible,
                     /*is_current_url=*/false);
}

// Tests that restarting while the Optimization Guide runs reports the old
// result as not current.
TEST_F(EligibilityDeciderTest, StartDuringOptimizationGuideReportsNotCurrent) {
  CreateDecider(&optimization_guide_);
  optimization_guide::OptimizationGuideDecisionCallback og_callback;
  EXPECT_CALL(
      optimization_guide_,
      CanApplyOptimization(
          article_url_, optimization_guide::proto::READER_MODE_ELIGIBLE,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&og_callback));
  StartAndWait(article_url_);
  delegate_.RunScriptCallback(&true_value_);
  ASSERT_TRUE(og_callback);

  GURL other_url("https://example.com/other");
  delegate_.last_committed_url = other_url;
  decider_->Start(other_url);
  std::move(og_callback)
      .Run(OptimizationGuideDecision::kTrue,
           optimization_guide::OptimizationMetadata());

  ExpectSingleResult(article_url_, ReaderModeEligibility::kReaderModeEligible,
                     /*is_current_url=*/false);
}

// Tests that the decision is dropped when there is no main frame to run the
// script in.
TEST_F(EligibilityDeciderTest, NoMainFrameDropsDecision) {
  CreateDecider(&optimization_guide_);
  delegate_.has_main_frame = false;

  StartAndWait(article_url_);

  EXPECT_EQ(1, delegate_.script_run_count);
  EXPECT_TRUE(results_.empty());
}

// Tests that destroying the decider while the script runs drops the result.
TEST_F(EligibilityDeciderTest, DestroyDuringScriptDropsResult) {
  CreateDecider(&optimization_guide_);
  StartAndWait(article_url_);

  decider_.reset();
  delegate_.RunScriptCallback(&true_value_);

  EXPECT_TRUE(results_.empty());
}

// Tests that destroying the decider while the Optimization Guide runs drops
// the result.
TEST_F(EligibilityDeciderTest, DestroyDuringOptimizationGuideDropsResult) {
  CreateDecider(&optimization_guide_);
  optimization_guide::OptimizationGuideDecisionCallback og_callback;
  EXPECT_CALL(
      optimization_guide_,
      CanApplyOptimization(
          article_url_, optimization_guide::proto::READER_MODE_ELIGIBLE,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(MoveArg<2>(&og_callback));
  StartAndWait(article_url_);
  delegate_.RunScriptCallback(&true_value_);
  ASSERT_TRUE(og_callback);

  decider_.reset();
  std::move(og_callback)
      .Run(OptimizationGuideDecision::kTrue,
           optimization_guide::OptimizationMetadata());

  EXPECT_TRUE(results_.empty());
}

}  // namespace

}  // namespace web_distiller
