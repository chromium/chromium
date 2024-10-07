// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace payments::facilitated {
namespace {

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
  MOCK_METHOD(bool, IsInLandscapeMode, (), (override));
};

class MockEwalletManager : public EwalletManager {
 public:
  MockEwalletManager() = default;
  ~MockEwalletManager() override = default;

  MOCK_METHOD(void,
              TriggerEwalletPushPayment,
              (const GURL&, const GURL&),
              (override));
};

class MockSecurityChecker : public SecurityChecker {
 public:
  MockSecurityChecker() = default;
  ~MockSecurityChecker() override = default;

  MOCK_METHOD(bool,
              IsSecureForPaymentLinkHandling,
              (content::RenderFrameHost&),
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

    std::unique_ptr<MockSecurityChecker> sc =
        std::make_unique<testing::NiceMock<MockSecurityChecker>>();
    security_checker_ = sc.get();
    driver_ = std::make_unique<ContentFacilitatedPaymentsDriver>(
        client_.get(), decider_.get(), render_frame_host, std::move(sc));

    std::unique_ptr<MockEwalletManager> em =
        std::make_unique<testing::NiceMock<MockEwalletManager>>();
    ewallet_manager_ = em.get();
    driver_->SetEwalletManagerForTesting(std::move(em));
  }

  void TearDown() override {
    decider_.reset();
    agent_.reset();
    driver_.reset();
    security_checker_ = nullptr;
    ewallet_manager_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider> decider_;
  std::unique_ptr<FacilitatedPaymentsClient> client_;
  std::unique_ptr<FakeFacilitatedPaymentsAgent> agent_;
  std::unique_ptr<ContentFacilitatedPaymentsDriver> driver_;
  raw_ptr<MockEwalletManager> ewallet_manager_;
  raw_ptr<MockSecurityChecker> security_checker_;
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

TEST_F(ContentFacilitatedPaymentsDriverTest, EwalletPushPaymentTriggered) {
  const GURL kFakePaymentLinkUrl("https://www.example.com/pay");

  EXPECT_CALL(*security_checker_, IsSecureForPaymentLinkHandling(testing::_))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(*ewallet_manager_, TriggerEwalletPushPayment).Times(1);

  driver_->HandlePaymentLink(kFakePaymentLinkUrl);
}

TEST_F(ContentFacilitatedPaymentsDriverTest,
       SecurityCheckFailed_EwalletPushPaymentNotTriggered) {
  const GURL kFakePaymentLinkUrl("https://www.example.com/pay");

  EXPECT_CALL(*security_checker_, IsSecureForPaymentLinkHandling(testing::_))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(*ewallet_manager_, TriggerEwalletPushPayment).Times(0);

  driver_->HandlePaymentLink(kFakePaymentLinkUrl);
}

TEST_F(ContentFacilitatedPaymentsDriverTest,
       NotInPrimaryMainFrame_EwalletPushPaymentNotTriggered) {
  // The first navigation navigates the initial empty document to the specified
  // URL in the main frame. And the subsequent navigation creates a new main
  // frame.
  NavigateAndCommit(GURL("https://www.example1.com"));
  NavigateAndCommit(GURL("https://www.example2.com"));
  const GURL kFakePaymentLinkUrl("https://www.example.com/pay");

  EXPECT_CALL(*security_checker_, IsSecureForPaymentLinkHandling).Times(0);
  EXPECT_CALL(*ewallet_manager_, TriggerEwalletPushPayment).Times(0);

  driver_->HandlePaymentLink(kFakePaymentLinkUrl);
}

}  // namespace
}  // namespace payments::facilitated
