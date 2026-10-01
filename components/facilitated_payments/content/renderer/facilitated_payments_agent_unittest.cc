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
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-shared.h"
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

// Returns signals with only the given fields set, so that each test can
// check that the exact struct built by the agent reaches the driver.
mojom::HeuristicSignalsPtr MakeSignals(bool has_square_candidate,
                                       bool url_keyword_match,
                                       bool text_keyword_match) {
  return mojom::HeuristicSignals::New(
      /*has_facilitated_payment_link=*/false, has_square_candidate,
      url_keyword_match, text_keyword_match);
}

class FakeFacilitatedPaymentsDriver : public mojom::FacilitatedPaymentsDriver {
 public:
  FakeFacilitatedPaymentsDriver() = default;
  ~FakeFacilitatedPaymentsDriver() override = default;

  void ReportHeuristicSignals(mojom::HeuristicSignalsPtr signals) override {
    reported_signals_.push_back(std::move(signals));
  }

  const std::vector<mojom::HeuristicSignalsPtr>& reported_signals() const {
    return reported_signals_;
  }

  void Bind(mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsDriver>
                receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

 private:
  std::vector<mojom::HeuristicSignalsPtr> reported_signals_;
  mojo::AssociatedReceiver<mojom::FacilitatedPaymentsDriver> receiver_{this};
};

class TestFacilitatedPaymentsAgent : public FacilitatedPaymentsAgent {
 public:
  explicit TestFacilitatedPaymentsAgent(
      blink::AssociatedInterfaceRegistry* registry)
      : FacilitatedPaymentsAgent(nullptr, registry) {}

  void SetSignalsToReturn(mojom::HeuristicSignalsPtr signals) {
    signals_to_return_ = std::move(signals);
  }

 protected:
  mojom::HeuristicSignalsPtr CalculateHeuristicSignals() override {
    return signals_to_return_.Clone();
  }

 private:
  mojom::HeuristicSignalsPtr signals_to_return_ =
      mojom::HeuristicSignals::New();
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
    // The agent starts dormant in production; these tests run the enabled
    // path, so they opt in explicitly.
    agent_->SetQrCodeDetectionEnabled(true);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  blink::AssociatedInterfaceRegistry registry_;
  FakeFacilitatedPaymentsDriver driver_;
  std::unique_ptr<TestFacilitatedPaymentsAgent> agent_;
};

// Test that `DidMeaningfulLayout` schedules a rescan that reports the signals
// only after the debounce delay has elapsed.
TEST_F(FacilitatedPaymentsAgentTest,
       TestDidMeaningfulLayoutTriggersDebouncedScan) {
  agent_->SetSignalsToReturn(MakeSignals(true, true, false));
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);

  // Fast-forward to just before the debounce delay elapses.
  task_environment_.FastForwardBy(kDebounceDelay - kEpsilon);
  EXPECT_TRUE(driver_.reported_signals().empty());

  // Fast-forward past the debounce delay.
  task_environment_.FastForwardBy(2 * kEpsilon);
  ASSERT_EQ(driver_.reported_signals().size(), 1u);
  EXPECT_EQ(driver_.reported_signals()[0], MakeSignals(true, true, false));
}

// Test that `DidChangeScrollOffset` resets the debounce timer so that rapidly
// firing scroll events do not trigger multiple scans.
TEST_F(FacilitatedPaymentsAgentTest,
       TestDidChangeScrollOffsetResetsDebounceTimer) {
  agent_->SetSignalsToReturn(MakeSignals(true, false, true));
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);

  // Scroll just before the timer would fire, which should reset it.
  task_environment_.FastForwardBy(kDebounceDelay - kEpsilon);
  agent_->DidChangeScrollOffset(blink::mojom::ScrollType::kUser);

  // Advance to just before the debounce delay since the scroll event. No
  // signals should be reported even though more than one delay has elapsed in
  // total.
  task_environment_.FastForwardBy(kDebounceDelay - kEpsilon);
  EXPECT_TRUE(driver_.reported_signals().empty());

  // Advance past the debounce delay since the scroll event.
  task_environment_.FastForwardBy(2 * kEpsilon);
  ASSERT_EQ(driver_.reported_signals().size(), 1u);
  EXPECT_EQ(driver_.reported_signals()[0], MakeSignals(true, false, true));
}

// Test that `DidFinishLoad` triggers a debounced heuristic rescan.
TEST_F(FacilitatedPaymentsAgentTest, TestDidFinishLoadTriggersDebouncedScan) {
  agent_->SetSignalsToReturn(MakeSignals(true, true, true));
  agent_->DidFinishLoad();

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  ASSERT_EQ(driver_.reported_signals().size(), 1u);
  EXPECT_EQ(driver_.reported_signals()[0], MakeSignals(true, true, true));
}

// Test that `DidFinishSameDocumentNavigation` triggers a debounced rescan.
TEST_F(FacilitatedPaymentsAgentTest,
       TestDidFinishSameDocumentNavigationTriggersDebouncedScan) {
  agent_->SetSignalsToReturn(MakeSignals(false, true, true));
  agent_->DidFinishSameDocumentNavigation();

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  ASSERT_EQ(driver_.reported_signals().size(), 1u);
  EXPECT_EQ(driver_.reported_signals()[0], MakeSignals(false, true, true));
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
  agent_->DidChangeScrollOffset(blink::mojom::ScrollType::kUser);
  EXPECT_FALSE(agent_->GetTimerForTesting().IsRunning());

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  EXPECT_TRUE(driver_.reported_signals().empty());

  // Re-enabling detection allows subsequent events to trigger rescan.
  agent_->SetQrCodeDetectionEnabled(true);
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);
  EXPECT_TRUE(agent_->GetTimerForTesting().IsRunning());

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  ASSERT_EQ(driver_.reported_signals().size(), 1u);
}

// Test that `OnDestruct` safely cleans up and cancels any pending rescan.
TEST_F(FacilitatedPaymentsAgentTest, TestOnDestructCancelsRescan) {
  agent_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);
  EXPECT_TRUE(agent_->GetTimerForTesting().IsRunning());

  // Release unique_ptr ownership before OnDestruct to avoid double-free.
  TestFacilitatedPaymentsAgent* raw_agent = agent_.release();
  raw_agent->OnDestruct();

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  EXPECT_TRUE(driver_.reported_signals().empty());
}

// Test that a new agent is dormant, so that lifecycle events on sites the
// browser has not opted in never schedule a scan.
TEST_F(FacilitatedPaymentsAgentTest, TestAgentStartsDormant) {
  // Built locally because `SetUp` opts `agent_` in. The registry is local too,
  // since registering the same interface twice in `registry_` fails a DCHECK.
  blink::AssociatedInterfaceRegistry dormant_registry;
  TestFacilitatedPaymentsAgent dormant_agent(&dormant_registry);
  mojo::AssociatedRemote<mojom::FacilitatedPaymentsDriver> driver_remote;
  FakeFacilitatedPaymentsDriver dormant_driver;
  dormant_driver.Bind(driver_remote.BindNewEndpointAndPassDedicatedReceiver());
  dormant_agent.SetDriverForTesting(std::move(driver_remote));

  EXPECT_FALSE(dormant_agent.is_qr_code_detection_enabled());

  dormant_agent.DidFinishLoad();
  dormant_agent.DidMeaningfulLayout(
      blink::WebMeaningfulLayout::kVisuallyNonEmpty);
  EXPECT_FALSE(dormant_agent.GetTimerForTesting().IsRunning());

  task_environment_.FastForwardBy(kDebounceDelay + kEpsilon);
  EXPECT_TRUE(dormant_driver.reported_signals().empty());
}

}  // namespace
}  // namespace payments::facilitated
