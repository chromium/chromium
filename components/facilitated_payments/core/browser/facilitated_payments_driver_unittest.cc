// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#include "components/optimization_guide/core/test_optimization_guide_decider.h"
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

class FacilitatedPaymentsDriverTest : public testing::Test {
 public:
  FacilitatedPaymentsDriverTest() {
    FacilitatedPaymentsApiClientCreator api_client_creator =
        base::BindRepeating(&MockFacilitatedPaymentsApiClient::CreateApiClient);
    driver_ = std::make_unique<FacilitatedPaymentsDriver>(&client_,
                                                          api_client_creator);
    std::unique_ptr<MockPixManager> pix_manager =
        std::make_unique<testing::NiceMock<MockPixManager>>(
            &client_, api_client_creator, &decider_);
    pix_manager_ = pix_manager.get();
    driver_->SetPixManagerForTesting(std::move(pix_manager));
  }

  ~FacilitatedPaymentsDriverTest() override = default;

 protected:
  optimization_guide::TestOptimizationGuideDecider decider_;
  MockFacilitatedPaymentsClient client_;
  std::unique_ptr<FacilitatedPaymentsDriver> driver_;
  raw_ptr<MockPixManager> pix_manager_;
};

TEST_F(FacilitatedPaymentsDriverTest,
       PixIdentifierExists_OnPixCodeCopiedToClipboardTriggered) {
  GURL url("https://example.com/");

  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard);

  // "0014br.gov.bcb.pix" is the Pix identifier.
  driver_->OnTextCopiedToClipboard(
      /*render_frame_host_url=*/url, /*copied_text=*/
      u"00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      /*ukm_source_id=*/123);
}

TEST_F(FacilitatedPaymentsDriverTest,
       PixIdentifierAbsent_OnPixCodeCopiedToClipboardNotTriggered) {
  GURL url("https://example.com/");

  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard).Times(0);

  driver_->OnTextCopiedToClipboard(
      /*render_frame_host_url=*/url, /*copied_text=*/u"notAValidPixIdentifier",
      /*ukm_source_id=*/123);
}

}  // namespace

}  // namespace payments::facilitated
