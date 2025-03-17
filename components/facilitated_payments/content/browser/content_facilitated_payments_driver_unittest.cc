// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "components/facilitated_payments/content/browser/facilitated_payments_api_client_factory.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments::facilitated {
namespace {

class MockPixManager : public PixManager {
 public:
  MockPixManager(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
      : PixManager(client,
                   std::move(api_client_creator),
                   optimization_guide_decider) {}
  ~MockPixManager() override = default;

  MOCK_METHOD(void,
              OnPixCodeCopiedToClipboard,
              (const GURL&, const std::string&, ukm::SourceId),
              (override));
};

class MockEwalletManager : public EwalletManager {
 public:
  MockEwalletManager(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
      : EwalletManager(client,
                       std::move(api_client_creator),
                       optimization_guide_decider) {}
  ~MockEwalletManager() override = default;

  MOCK_METHOD(void,
              TriggerEwalletPushPayment,
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
        client_.get(), decider_.get(), render_frame_host, std::move(sc));
    std::unique_ptr<MockEwalletManager> em =
        std::make_unique<testing::NiceMock<MockEwalletManager>>(
            client_.get(),
            GetFacilitatedPaymentsApiClientCreator(
                render_frame_host->GetGlobalId()),
            decider_.get());
    ewallet_manager_ = em.get();
    std::unique_ptr<MockPixManager> pix_manager =
        std::make_unique<testing::NiceMock<MockPixManager>>(
            client_.get(),
            GetFacilitatedPaymentsApiClientCreator(
                render_frame_host->GetGlobalId()),
            decider_.get());
    pix_manager_ = pix_manager.get();

    driver_->SetPixManagerForTesting(std::move(pix_manager));
    driver_->SetEwalletManagerForTesting(std::move(em));
  }

  void TearDown() override {
    decider_.reset();
    driver_.reset();
    security_checker_ = nullptr;
    pix_manager_ = nullptr;
    ewallet_manager_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider> decider_;
  std::unique_ptr<FacilitatedPaymentsClient> client_;
  std::unique_ptr<ContentFacilitatedPaymentsDriver> driver_;
  raw_ptr<MockPixManager> pix_manager_;
  raw_ptr<MockEwalletManager> ewallet_manager_;
  raw_ptr<MockSecurityChecker> security_checker_;
};

TEST_F(ContentFacilitatedPaymentsDriverTest,
       PixIdentifierExists_OnPixCodeCopiedToClipboardTriggered) {
  GURL url("https://example.com/");

  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard);

  // "0014br.gov.bcb.pix" is the Pix identifier.
  driver_->OnTextCopiedToClipboard(
      /*render_frame_host_url=*/url, /*copied_text=*/
      u"00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      /*ukm_source_id=*/123);
}

TEST_F(ContentFacilitatedPaymentsDriverTest,
       PixIdentifierAbsent_OnPixCodeCopiedToClipboardNotTriggered) {
  GURL url("https://example.com/");

  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard).Times(0);

  driver_->OnTextCopiedToClipboard(
      /*render_frame_host_url=*/url, /*copied_text=*/u"notAValidPixIdentifier",
      /*ukm_source_id=*/123);
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
