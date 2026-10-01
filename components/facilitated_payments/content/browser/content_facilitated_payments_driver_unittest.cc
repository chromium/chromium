// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"
#include "components/facilitated_payments/content/browser/facilitated_payments_api_client_factory.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "components/optimization_guide/core/hints/test_optimization_guide_decider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace payments::facilitated {
namespace {

// Matches a `mojom::HeuristicSignals` equal to `*expected`. Mojo structs are
// move-only, so `testing::Eq()` cannot hold a copy of the expected value.
MATCHER_P(SignalsEq, expected, "") {
  return arg.Equals(*expected);
}

class MockContentFacilitatedPaymentsDriverFactory
    : public ContentFacilitatedPaymentsDriverFactory {
 public:
  MockContentFacilitatedPaymentsDriverFactory(
      content::WebContents* web_contents,
      FacilitatedPaymentsClient* client)
      : ContentFacilitatedPaymentsDriverFactory(web_contents, client) {}
  ~MockContentFacilitatedPaymentsDriverFactory() override = default;

  MOCK_METHOD(void,
              OnHeuristicSignalsReported,
              (content::RenderFrameHost*, const mojom::HeuristicSignals&),
              (override));
};

class MockFacilitatedPaymentsAgent : public mojom::FacilitatedPaymentsAgent {
 public:
  MockFacilitatedPaymentsAgent() = default;
  ~MockFacilitatedPaymentsAgent() override = default;

  MOCK_METHOD(void, SetQrCodeDetectionEnabled, (bool), (override));
};

#if BUILDFLAG(IS_ANDROID)
constexpr char16_t kFakePixCode[] =
    u"00020126370014br.gov.bcb.pix2515www.example.com6304EA3F";

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
              (const GURL&,
               const std::optional<GURL>&,
               const url::Origin&,
               bool,
               PixCodeRustValidationResult,
               std::string,
               ukm::SourceId),
              (override));
};

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
#endif  // BUILDFLAG(IS_ANDROID)

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
    factory_ = std::make_unique<
        testing::NiceMock<MockContentFacilitatedPaymentsDriverFactory>>(
        web_contents(), client_.get());
    driver_ = std::make_unique<ContentFacilitatedPaymentsDriver>(
        client_.get(), render_frame_host, std::move(sc), factory_.get());
#if BUILDFLAG(IS_ANDROID)
    std::unique_ptr<MockPaymentLinkManager> em =
        std::make_unique<testing::NiceMock<MockPaymentLinkManager>>(
            client_.get(),
            GetFacilitatedPaymentsApiClientCreator(
                render_frame_host->GetGlobalId()),
            decider_.get());
    payment_link_manager_ = em.get();
    driver_->SetPaymentLinkManagerForTesting(std::move(em));

    std::unique_ptr<MockPixManager> pm =
        std::make_unique<testing::NiceMock<MockPixManager>>(
            client_.get(),
            GetFacilitatedPaymentsApiClientCreator(
                render_frame_host->GetGlobalId()),
            decider_.get());
    pix_manager_ = pm.get();
    driver_->SetPixManagerForTesting(std::move(pm));
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void TearDown() override {
    SetContents(nullptr);
    security_checker_ = nullptr;
#if BUILDFLAG(IS_ANDROID)
    payment_link_manager_ = nullptr;
    pix_manager_ = nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
    // The driver owns the managers, which hold references to `decider_`, so it
    // must be destroyed before `decider_`.
    driver_.reset();
    factory_.reset();
    decider_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider> decider_;
  std::unique_ptr<FacilitatedPaymentsClient> client_;
  std::unique_ptr<MockContentFacilitatedPaymentsDriverFactory> factory_;
  std::unique_ptr<ContentFacilitatedPaymentsDriver> driver_;
#if BUILDFLAG(IS_ANDROID)
  raw_ptr<MockPaymentLinkManager> payment_link_manager_;
  raw_ptr<MockPixManager> pix_manager_;
#endif  // BUILDFLAG(IS_ANDROID)
  raw_ptr<MockSecurityChecker> security_checker_;
};

// Payment link and Pix flows are only supported on Android.
#if BUILDFLAG(IS_ANDROID)
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

TEST_F(ContentFacilitatedPaymentsDriverTest,
       PixCodeCopied_ForwardedToPixManager) {
  EXPECT_CALL(*security_checker_,
              IsSecureForPaymentLinkHandling(testing::Ref(*main_rfh())))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard).Times(1);

  driver_->OnTextCopiedToClipboard(
      GURL("https://example.com"), std::nullopt,
      url::Origin::Create(GURL("https://example.com")), kFakePixCode,
      ukm::kInvalidSourceId, /*is_same_origin=*/false);
}

TEST_F(ContentFacilitatedPaymentsDriverTest,
       SecurityCheckFailed_PixCodeBlocked) {
  EXPECT_CALL(*security_checker_,
              IsSecureForPaymentLinkHandling(testing::Ref(*main_rfh())))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard).Times(0);

  driver_->OnTextCopiedToClipboard(
      GURL("http://example.com"), std::nullopt,
      url::Origin::Create(GURL("http://example.com")), kFakePixCode,
      ukm::kInvalidSourceId, /*is_same_origin=*/false);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Test that reported heuristic signals are forwarded to the factory.
TEST_F(ContentFacilitatedPaymentsDriverTest,
       ReportHeuristicSignals_ForwardsToFactory) {
  mojom::HeuristicSignalsPtr signals = mojom::HeuristicSignals::New();
  signals->has_square_candidate = true;
  signals->text_keyword_match = true;
  EXPECT_CALL(*factory_,
              OnHeuristicSignalsReported(main_rfh(), SignalsEq(signals.get())))
      .Times(1);

  driver_->ReportHeuristicSignals(signals.Clone());
}

// Test binding the `FacilitatedPaymentsDriver` associated receiver and calling
// `ReportHeuristicSignals` through mojo.
TEST_F(ContentFacilitatedPaymentsDriverTest,
       SetFacilitatedPaymentsDriverReceiver) {
  mojo::AssociatedRemote<mojom::FacilitatedPaymentsDriver> remote;
  driver_->SetFacilitatedPaymentsDriverReceiver(
      remote.BindNewEndpointAndPassDedicatedReceiver());

  mojom::HeuristicSignalsPtr signals = mojom::HeuristicSignals::New();
  signals->has_facilitated_payment_link = true;
  signals->has_square_candidate = true;
  signals->url_keyword_match = true;
  base::test::TestFuture<void> future;
  EXPECT_CALL(*factory_,
              OnHeuristicSignalsReported(main_rfh(), SignalsEq(signals.get())))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));

  remote->ReportHeuristicSignals(signals.Clone());
  EXPECT_TRUE(future.Wait());
}

// Test getting and overriding the FacilitatedPaymentsAgent remote.
TEST_F(ContentFacilitatedPaymentsDriverTest,
       GetAndSetFacilitatedPaymentsAgent) {
  mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent> remote;
  MockFacilitatedPaymentsAgent mock_agent;
  mojo::AssociatedReceiver<mojom::FacilitatedPaymentsAgent> receiver(
      &mock_agent, remote.BindNewEndpointAndPassDedicatedReceiver());

  driver_->SetFacilitatedPaymentsAgentForTesting(std::move(remote));

  base::test::TestFuture<void> future;
  EXPECT_CALL(mock_agent, SetQrCodeDetectionEnabled(true))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  driver_->GetFacilitatedPaymentsAgent()->SetQrCodeDetectionEnabled(true);
  EXPECT_TRUE(future.Wait());
}

}  // namespace
}  // namespace payments::facilitated
