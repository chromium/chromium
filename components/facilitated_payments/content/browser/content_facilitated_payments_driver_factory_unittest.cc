// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/facilitated_payments/content/browser/security_checker.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/test_optimization_guide_decider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments::facilitated {

class MockContentFacilitatedPaymentsDriver
    : public ContentFacilitatedPaymentsDriver {
 public:
  MockContentFacilitatedPaymentsDriver(
      FacilitatedPaymentsClient* client,
      content::RenderFrameHost* rfh,
      std::unique_ptr<SecurityChecker> security_checker)
      : ContentFacilitatedPaymentsDriver(client,
                                         rfh,
                                         std::move(security_checker)) {}

  MOCK_METHOD(void,
              OnTextCopiedToClipboard,
              (const GURL& main_frame_url,
               const std::optional<GURL>& iframe_url,
               const url::Origin& main_frame_origin,
               const std::u16string& copied_text,
               ukm::SourceId ukm_source_id,
               bool is_same_origin),
              (override));
};

class ContentFacilitatedPaymentsDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    decider_ =
        std::make_unique<optimization_guide::TestOptimizationGuideDecider>();
    client_ = std::make_unique<MockFacilitatedPaymentsClient>();
    // Every committed main frame navigation consults the merchant allowlist, so
    // the decider must be available by default.
    ON_CALL(*client_, GetOptimizationGuideDecider)
        .WillByDefault(testing::Return(decider_.get()));
    factory_ = std::make_unique<ContentFacilitatedPaymentsDriverFactory>(
        web_contents(), client_.get());
  }

  void TearDown() override {
    SetContents(nullptr);

    factory_.reset();
    client_.reset();
    decider_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<optimization_guide::TestOptimizationGuideDecider> decider_;
  optimization_guide::MockOptimizationGuideDecider mock_decider_;
  std::unique_ptr<MockFacilitatedPaymentsClient> client_;
  std::unique_ptr<ContentFacilitatedPaymentsDriverFactory> factory_;
};

TEST_F(
    ContentFacilitatedPaymentsDriverFactoryTest,
    OnTextCopiedToClipboard_PixCodeInIFrame_FlagEnabled_PixFlowExitedReasonNotLogged) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableIframeForPix);
  base::HistogramTester histogram_tester;

  ON_CALL(*client_, GetOptimizationGuideDecider)
      .WillByDefault(testing::Return(decider_.get()));

  NavigateAndCommit(GURL("https://example.com"));
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("iframe");

  const std::u16string kValidPixCode = u"00020126180014br.gov.bcb.pix63041D3D";

  factory_->OnTextCopiedToClipboard(iframe, kValidPixCode);

  histogram_tester.ExpectBucketCount(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kPixCodeInIFrame,
      /*expected_count=*/0);
}

TEST_F(
    ContentFacilitatedPaymentsDriverFactoryTest,
    OnTextCopiedToClipboard_PixCodeInIFrame_FlagEnabled_CorrectIframeUrlPassedToDriver) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableIframeForPix);

  ON_CALL(*client_, GetOptimizationGuideDecider)
      .WillByDefault(testing::Return(decider_.get()));

  NavigateAndCommit(GURL("https://example.com"));
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("iframe");
  const std::u16string kValidPixCode = u"00020126180014br.gov.bcb.pix63041D3D";

  auto mock_driver = std::make_unique<MockContentFacilitatedPaymentsDriver>(
      client_.get(), iframe, std::make_unique<SecurityChecker>());
  MockContentFacilitatedPaymentsDriver* mock_driver_ptr = mock_driver.get();
  factory_->driver_map_[iframe] = std::move(mock_driver);

  // Verify the driver receives the correct iframe URL.
  EXPECT_CALL(
      *mock_driver_ptr,
      OnTextCopiedToClipboard(
          /*main_frame_url=*/main_frame->GetLastCommittedURL(),
          /*iframe_url=*/std::make_optional(iframe->GetLastCommittedURL()),
          /*main_frame_origin=*/main_frame->GetLastCommittedOrigin(),
          kValidPixCode, iframe->GetPageUkmSourceId(),
          /*is_same_origin=*/testing::_));

  factory_->OnTextCopiedToClipboard(iframe, kValidPixCode);
}

TEST_F(
    ContentFacilitatedPaymentsDriverFactoryTest,
    OnTextCopiedToClipboard_FrameNotActive_DoesNotTriggerPixDetection_PixFlowExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://example.com/initial"));
  content::RenderFrameHost* initial_rfh = web_contents()->GetPrimaryMainFrame();
  NavigateAndCommit(GURL("https://example.com/final"));
  ASSERT_FALSE(initial_rfh->IsActive());

  const std::u16string kValidPixCode = u"00020126180014br.gov.bcb.pix63041D3D";

  // Expect that the client is not called because the frame is inactive.
  EXPECT_CALL(*client_, ShowPixPaymentPrompt).Times(0);

  factory_->OnTextCopiedToClipboard(initial_rfh, kValidPixCode);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kFrameNotActive,
      /*expected_bucket_count=*/1);
}

TEST_F(
    ContentFacilitatedPaymentsDriverFactoryTest,
    OnTextCopiedToClipboard_ErrorDocument_DoesNotTriggerPixDetection_PixFlowExitedReasonLogged) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableIframeForPix);

  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://example.com"));
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("iframe");

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.com/error"), iframe);
  simulator->Fail(net::ERR_BLOCKED_BY_CLIENT);
  simulator->CommitErrorPage();
  content::RenderFrameHost* error_frame = simulator->GetFinalRenderFrameHost();
  ASSERT_TRUE(error_frame->IsErrorDocument());

  const std::u16string kValidPixCode = u"00020126180014br.gov.bcb.pix63041D3D";

  // Expect that the client is not called because the frame is an error
  // document.
  EXPECT_CALL(*client_, ShowPixPaymentPrompt).Times(0);

  factory_->OnTextCopiedToClipboard(error_frame, kValidPixCode);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kFrameIsErrorDocument,
      /*expected_bucket_count=*/1);
}

// Test that QR code detection is eligible when the merchant is on the
// allowlist and the feature is enabled.
TEST_F(
    ContentFacilitatedPaymentsDriverFactoryTest,
    IsEligibleForQrCodeDetection_MerchantAllowlistedAndFeatureEnabled_ReturnsTrue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableDesktopQrCodeDetection);

  const GURL kMerchantUrl("https://allowlisted-merchant.com");
  ON_CALL(*client_, GetOptimizationGuideDecider)
      .WillByDefault(testing::Return(&mock_decider_));
  EXPECT_CALL(mock_decider_,
              CanApplyOptimization(
                  testing::Eq(kMerchantUrl),
                  testing::Eq(optimization_guide::proto::
                                  PAYMENT_QR_CODE_MERCHANT_URL_REGEX_ALLOWLIST),
                  testing::A<optimization_guide::OptimizationMetadata*>()))
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_TRUE(factory_->IsEligibleForQrCodeDetection(kMerchantUrl));
}

// Test that a merchant missing from the allowlist is not eligible even when the
// feature is enabled.
TEST_F(ContentFacilitatedPaymentsDriverFactoryTest,
       IsEligibleForQrCodeDetection_MerchantNotAllowlisted_ReturnsFalse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableDesktopQrCodeDetection);

  const GURL kMerchantUrl("https://unknown-merchant.com");
  ON_CALL(*client_, GetOptimizationGuideDecider)
      .WillByDefault(testing::Return(&mock_decider_));
  EXPECT_CALL(mock_decider_,
              CanApplyOptimization(
                  testing::Eq(kMerchantUrl), testing::_,
                  testing::A<optimization_guide::OptimizationMetadata*>()))
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_FALSE(factory_->IsEligibleForQrCodeDetection(kMerchantUrl));
}

// Test that a disabled feature short circuits before the allowlist is queried.
TEST_F(ContentFacilitatedPaymentsDriverFactoryTest,
       IsEligibleForQrCodeDetection_FeatureDisabled_ReturnsFalse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnableDesktopQrCodeDetection);

  ON_CALL(*client_, GetOptimizationGuideDecider)
      .WillByDefault(testing::Return(&mock_decider_));
  EXPECT_CALL(mock_decider_,
              CanApplyOptimization(
                  testing::_, testing::_,
                  testing::A<optimization_guide::OptimizationMetadata*>()))
      .Times(0);

  EXPECT_FALSE(factory_->IsEligibleForQrCodeDetection(
      GURL("https://allowlisted-merchant.com")));
}

// Test that detection is not eligible when the embedder provides no
// Optimization Guide decider.
TEST_F(ContentFacilitatedPaymentsDriverFactoryTest,
       IsEligibleForQrCodeDetection_NoOptimizationGuideDecider_ReturnsFalse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableDesktopQrCodeDetection);

  ON_CALL(*client_, GetOptimizationGuideDecider)
      .WillByDefault(testing::Return(nullptr));

  EXPECT_FALSE(factory_->IsEligibleForQrCodeDetection(
      GURL("https://allowlisted-merchant.com")));
}

}  // namespace payments::facilitated
