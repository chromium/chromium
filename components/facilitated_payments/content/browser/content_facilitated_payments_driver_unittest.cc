// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace payments::facilitated {

class FakeFacilitatedPaymentsAgent : public mojom::FacilitatedPaymentsAgent {
 public:
  FakeFacilitatedPaymentsAgent() = default;
  ~FakeFacilitatedPaymentsAgent() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsAgent>(
            std::move(handle)));
  }

  // mojom::FacilitatedPaymentsAgent:
  MOCK_METHOD(void,
              TriggerPixCodeDetection,
              (base::OnceCallback<void(mojom::PixCodeDetectionResult,
                                       const std::string&)>),
              (override));

 private:
  mojo::AssociatedReceiver<mojom::FacilitatedPaymentsAgent> receiver_{this};
};

// A mock for the facilitated payment "client" interface, used for loading risk
// data, and showing the PIX payment prompt.
class FakeFacilitatedPaymentsClient : public FacilitatedPaymentsClient {
 public:
  FakeFacilitatedPaymentsClient() = default;
  ~FakeFacilitatedPaymentsClient() override = default;

  MOCK_METHOD(void,
              LoadRiskData,
              (base::OnceCallback<void(const std::string&)>),
              (override));
  MOCK_METHOD(autofill::PaymentsDataManager*,
              GetPaymentsDataManager,
              (),
              (override));
  MOCK_METHOD(FacilitatedPaymentsNetworkInterface*,
              GetFacilitatedPaymentsNetworkInterface,
              (),
              (override));
  MOCK_METHOD(std::optional<CoreAccountInfo>,
              GetCoreAccountInfo,
              (),
              (override));
};

class ContentFacilitatedPaymentsDriverTest
    : public content::RenderViewHostTestHarness,
      public ::testing::WithParamInterface<mojom::PixCodeDetectionResult> {
 public:
  // content::RenderViewHostTestHarness:
  void SetUp() override {
    decider_ =
        std::make_unique<optimization_guide::TestOptimizationGuideDecider>();
    client_ = std::make_unique<FakeFacilitatedPaymentsClient>();
    agent_ = std::make_unique<FakeFacilitatedPaymentsAgent>();
    content::RenderViewHostTestHarness::SetUp();

    // Set the fake agent as the Mojo receiver for the primary main document.
    content::RenderFrameHost* render_frame_host =
        RenderViewHostTestHarness::web_contents()->GetPrimaryMainFrame();
    render_frame_host->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::FacilitatedPaymentsAgent::Name_,
            base::BindRepeating(
                &FakeFacilitatedPaymentsAgent::BindPendingReceiver,
                base::Unretained(agent_.get())));

    driver_ = std::make_unique<ContentFacilitatedPaymentsDriver>(
        client_.get(), decider_.get(), render_frame_host);
  }

  void TearDown() override {
    decider_.reset();
    agent_.reset();
    driver_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider> decider_;
  std::unique_ptr<FacilitatedPaymentsClient> client_;
  std::unique_ptr<FakeFacilitatedPaymentsAgent> agent_;
  std::unique_ptr<ContentFacilitatedPaymentsDriver> driver_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContentFacilitatedPaymentsDriverTest,
    testing::Values(mojom::PixCodeDetectionResult::kPixCodeDetectionNotRun,
                    mojom::PixCodeDetectionResult::kPixCodeNotFound,
                    mojom::PixCodeDetectionResult::kInvalidPixCodeFound,
                    mojom::PixCodeDetectionResult::kValidPixCodeFound));

// Test that the renderer receives the call to run PIX code detection, and that
// the browser is able to get the result.
TEST_P(ContentFacilitatedPaymentsDriverTest, TestBrowserRendererConnection) {
  base::test::TestFuture<mojom::PixCodeDetectionResult, const std::string&>
      captured_result_for_test;

  // On the renderer, when the call to run PIX code detection is received, run
  // callback with different results.
  EXPECT_CALL(*agent_, TriggerPixCodeDetection)
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<0>(GetParam(), "pix code"));

  // Trigger PIX code detection from the browser, and capture the result that
  // the callback is called with.
  driver_->TriggerPixCodeDetection(captured_result_for_test.GetCallback());

  // Verify that the correct result is captured.
  EXPECT_EQ(captured_result_for_test.Get<mojom::PixCodeDetectionResult>(),
            GetParam());
  EXPECT_EQ(captured_result_for_test.Get<std::string>(),
            "pix code");
}

}  // namespace payments::facilitated
