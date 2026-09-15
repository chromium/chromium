// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/renderer/facilitated_payments_agent.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"

namespace payments::facilitated {
namespace {

// The debounce delay under test. Derived from the production constant so that
// these tests keep exercising the boundary if the delay ever changes.
constexpr base::TimeDelta kDebounceDelay =
    FacilitatedPaymentsAgent::kRescanDebounceDelay;

// Small offset used to step just before or just after `kDebounceDelay`, so the
// tests never depend on the absolute value of the delay.
constexpr base::TimeDelta kEpsilon = base::Milliseconds(1);

class FakeFacilitatedPaymentsDriver : public mojom::FacilitatedPaymentsDriver {
 public:
  FakeFacilitatedPaymentsDriver() = default;
  ~FakeFacilitatedPaymentsDriver() override = default;

  void ReportHeuristicScore(double score) override {
    reported_scores_.push_back(score);
  }

  const std::vector<double>& reported_scores() const {
    return reported_scores_;
  }

  void ClearReportedScores() { reported_scores_.clear(); }

  void Bind(mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsDriver>
                receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

 private:
  std::vector<double> reported_scores_;
  mojo::AssociatedReceiver<mojom::FacilitatedPaymentsDriver> receiver_{this};
};

class TestFacilitatedPaymentsAgent : public FacilitatedPaymentsAgent {
 public:
  explicit TestFacilitatedPaymentsAgent(
      blink::AssociatedInterfaceRegistry* registry)
      : FacilitatedPaymentsAgent(nullptr, registry) {}

  void SetScoreToReturn(double score) { score_to_return_ = score; }

 protected:
  double CalculateHeuristicScore() override { return score_to_return_; }

 private:
  double score_to_return_ = 0.5;
};

class FacilitatedPaymentsAgentTest : public testing::Test {
 public:
  FacilitatedPaymentsAgentTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FacilitatedPaymentsAgentTest() override = default;

  void SetUp() override {
    agent_ = std::make_unique<TestFacilitatedPaymentsAgent>(&registry_);
    mojo::AssociatedRemote<mojom::FacilitatedPaymentsDriver> driver_remote;
    driver_.Bind(driver_remote.BindNewEndpointAndPassDedicatedReceiver());
    agent_->SetDriverForTesting(std::move(driver_remote));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  blink::AssociatedInterfaceRegistry registry_;
  FakeFacilitatedPaymentsDriver driver_;
  std::unique_ptr<TestFacilitatedPaymentsAgent> agent_;
};

// Test that `DidMeaningfulLayout` schedules a rescan that reports the score
// only after the debounce delay has elapsed.
TEST_F(FacilitatedPaymentsAgentTest,
       TestDidMeaningfulLayoutTriggersDebouncedScan) {
  agent_->SetScoreToReturn(0.8);
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);

  // Fast-forward to just before the debounce delay elapses.
  task_environment_.FastForwardBy(kDebounceDelay - kEpsilon);
  EXPECT_TRUE(driver_.reported_scores().empty());

  // Fast-forward past the debounce delay.
  task_environment_.FastForwardBy(2 * kEpsilon);
  ASSERT_EQ(driver_.reported_scores().size(), 1u);
  EXPECT_DOUBLE_EQ(driver_.reported_scores()[0], 0.8);
}

// Test that `DidChangeScrollOffset` resets the debounce timer so that rapidly
// firing scroll events do not trigger multiple scans.
TEST_F(FacilitatedPaymentsAgentTest,
       TestDidChangeScrollOffsetResetsDebounceTimer) {
  agent_->SetScoreToReturn(0.7);
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);

  // Scroll just before the timer would fire, which should reset it.
  task_environment_.FastForwardBy(kDebounceDelay - kEpsilon);
  agent_->DidChangeScrollOffset();

  // Advance to just before the debounce delay since the scroll event. No score
  // should be reported even though more than one delay has elapsed in total.
  task_environment_.FastForwardBy(kDebounceDelay - kEpsilon);
  EXPECT_TRUE(driver_.reported_scores().empty());

  // Advance past the debounce delay since the scroll event.
  task_environment_.FastForwardBy(2 * kEpsilon);
  ASSERT_EQ(driver_.reported_scores().size(), 1u);
  EXPECT_DOUBLE_EQ(driver_.reported_scores()[0], 0.7);
}

// Test that `DidFinishLoad` triggers a debounced heuristic rescan.
TEST_F(FacilitatedPaymentsAgentTest, TestDidFinishLoadTriggersDebouncedScan) {
  agent_->SetScoreToReturn(0.9);
  agent_->DidFinishLoad();

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  ASSERT_EQ(driver_.reported_scores().size(), 1u);
  EXPECT_DOUBLE_EQ(driver_.reported_scores()[0], 0.9);
}

// Test that `DidFinishSameDocumentNavigation` triggers a debounced rescan.
TEST_F(FacilitatedPaymentsAgentTest,
       TestDidFinishSameDocumentNavigationTriggersDebouncedScan) {
  agent_->SetScoreToReturn(0.6);
  agent_->DidFinishSameDocumentNavigation();

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  ASSERT_EQ(driver_.reported_scores().size(), 1u);
  EXPECT_DOUBLE_EQ(driver_.reported_scores()[0], 0.6);
}

// Test that disabling detection cancels any running debounce timer and ignores
// further lifecycle events until re-enabled.
TEST_F(FacilitatedPaymentsAgentTest,
       TestSetQrCodeDetectionEnabledCancelsTimerAndPreventsScan) {
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);
  EXPECT_TRUE(agent_->GetTimerForTesting().IsRunning());

  // Disable detection: should cancel running timer.
  agent_->SetQrCodeDetectionEnabled(false);
  EXPECT_FALSE(agent_->GetTimerForTesting().IsRunning());

  // Further lifecycle events should be ignored while disabled.
  agent_->DidChangeScrollOffset();
  EXPECT_FALSE(agent_->GetTimerForTesting().IsRunning());

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  EXPECT_TRUE(driver_.reported_scores().empty());

  // Re-enabling detection allows subsequent events to trigger rescan.
  agent_->SetQrCodeDetectionEnabled(true);
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);
  EXPECT_TRUE(agent_->GetTimerForTesting().IsRunning());

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  ASSERT_EQ(driver_.reported_scores().size(), 1u);
}

// Test that `OnDestruct` safely cleans up and cancels any pending rescan.
TEST_F(FacilitatedPaymentsAgentTest, TestOnDestructCancelsRescan) {
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);
  EXPECT_TRUE(agent_->GetTimerForTesting().IsRunning());

  // Release unique_ptr ownership before OnDestruct to avoid double-free.
  TestFacilitatedPaymentsAgent* raw_agent = agent_.release();
  raw_agent->OnDestruct();

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  EXPECT_TRUE(driver_.reported_scores().empty());
}

}  // namespace
}  // namespace payments::facilitated
