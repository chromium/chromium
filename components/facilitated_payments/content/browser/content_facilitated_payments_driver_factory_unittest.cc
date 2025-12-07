// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments::facilitated {

class ContentFacilitatedPaymentsDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    client_ = std::make_unique<MockFacilitatedPaymentsClient>();
    factory_ = std::make_unique<ContentFacilitatedPaymentsDriverFactory>(
        web_contents(), client_.get());
  }

  void TearDown() override {
    SetContents(nullptr);

    factory_.reset();
    client_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<MockFacilitatedPaymentsClient> client_;
  std::unique_ptr<ContentFacilitatedPaymentsDriverFactory> factory_;
};

TEST_F(
    ContentFacilitatedPaymentsDriverFactoryTest,
    OnTextCopiedToClipboard_PixCodeInIFrame_DoesNotTriggerPixDetection_PixFlowExitedReasonLogged) {
  base::HistogramTester histogram_tester;
  NavigateAndCommit(GURL("https://example.com"));
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_frame)->AppendChild("subframe");

  const std::u16string kValidPixCode = u"00020126180014br.gov.bcb.pix63041D3D";

  // Expect that the client is not called because the copy happened in an
  // iframe.
  EXPECT_CALL(*client_, ShowPixPaymentPrompt).Times(0);

  factory_->OnTextCopiedToClipboard(subframe, kValidPixCode);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PixFlowExitedReason::kPixCodeInIFrame,
      /*expected_bucket_count=*/1);
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

}  // namespace payments::facilitated
