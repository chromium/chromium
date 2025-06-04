// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "components/facilitated_payments/content/browser/facilitated_payments_api_client_factory.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#include "components/optimization_guide/core/hints/test_optimization_guide_decider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments::facilitated {
namespace {

class MockPaymentLinkManager : public PaymentLinkManager {
 public:
  MockPaymentLinkManager(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
      : PaymentLinkManager(client,
                           std::move(api_client_creator),
                           optimization_guide_decider) {}
  ~MockPaymentLinkManager() override = default;

  MOCK_METHOD(void,
              TriggerPaymentLinkPushPayment,
              (const GURL&, const GURL&, ukm::SourceId),
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
    : public content::RenderViewHostTestHarness {
 public:
  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    decider_ =
        std::make_unique<optimization_guide::TestOptimizationGuideDecider>();
    client_ = std::make_unique<MockFacilitatedPaymentsClient>();
    content::RenderFrameHost* render_frame_host =
        RenderViewHostTestHarness::web_contents()->GetPrimaryMainFrame();
    std::unique_ptr<MockSecurityChecker> sc =
        std::make_unique<testing::NiceMock<MockSecurityChecker>>();
    security_checker_ = sc.get();
    driver_ = std::make_unique<ContentFacilitatedPaymentsDriver>(
        client_.get(), render_frame_host, std::move(sc));
    std::unique_ptr<MockPaymentLinkManager> em =
        std::make_unique<testing::NiceMock<MockPaymentLinkManager>>(
            client_.get(),
            GetFacilitatedPaymentsApiClientCreator(
                render_frame_host->GetGlobalId()),
            decider_.get());
    payment_link_manager_ = em.get();
    driver_->SetPaymentLinkManagerForTesting(std::move(em));
  }

  void TearDown() override {
    decider_.reset();
    driver_.reset();
    security_checker_ = nullptr;
    payment_link_manager_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider> decider_;
  std::unique_ptr<FacilitatedPaymentsClient> client_;
  std::unique_ptr<ContentFacilitatedPaymentsDriver> driver_;
  raw_ptr<MockPaymentLinkManager> payment_link_manager_;
  raw_ptr<MockSecurityChecker> security_checker_;
};

TEST_F(ContentFacilitatedPaymentsDriverTest, PaymentLinkPushPaymentTriggered) {
  const GURL kFakePaymentLinkUrl("https://www.example.com/pay");

  EXPECT_CALL(*security_checker_, IsSecureForPaymentLinkHandling(testing::_))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(*payment_link_manager_, TriggerPaymentLinkPushPayment).Times(1);

  driver_->HandlePaymentLink(kFakePaymentLinkUrl);
}

TEST_F(ContentFacilitatedPaymentsDriverTest,
       SecurityCheckFailed_PaymentLinkPushPaymentNotTriggered) {
  const GURL kFakePaymentLinkUrl("https://www.example.com/pay");

  EXPECT_CALL(*security_checker_, IsSecureForPaymentLinkHandling(testing::_))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(*payment_link_manager_, TriggerPaymentLinkPushPayment).Times(0);

  driver_->HandlePaymentLink(kFakePaymentLinkUrl);
}

TEST_F(ContentFacilitatedPaymentsDriverTest,
       NotInPrimaryMainFrame_PaymentLinkPushPaymentNotTriggered) {
  // The first navigation navigates the initial empty document to the specified
  // URL in the main frame. And the subsequent navigation creates a new main
  // frame.
  NavigateAndCommit(GURL("https://www.example1.com"));
  NavigateAndCommit(GURL("https://www.example2.com"));
  const GURL kFakePaymentLinkUrl("https://www.example.com/pay");

  EXPECT_CALL(*security_checker_, IsSecureForPaymentLinkHandling).Times(0);
  EXPECT_CALL(*payment_link_manager_, TriggerPaymentLinkPushPayment).Times(0);

  driver_->HandlePaymentLink(kFakePaymentLinkUrl);
}

}  // namespace
}  // namespace payments::facilitated
