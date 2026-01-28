// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/optimization_guide/core/hints/test_optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

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
              (const GURL&,
               const std::optional<GURL>&,
               const url::Origin&,
               std::optional<PixCodeRustValidationResult>,
               std::string,
               ukm::SourceId),
              (override));
};

class FacilitatedPaymentsDriverTest : public testing::TestWithParam<bool> {
 public:
  FacilitatedPaymentsDriverTest() {
    scoped_feature_list_.InitWithFeatureState(kUseRustPixCodeValidator,
                                              GetParam());

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
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::TestOptimizationGuideDecider decider_;
  MockFacilitatedPaymentsClient client_;
  std::unique_ptr<FacilitatedPaymentsDriver> driver_;
  raw_ptr<MockPixManager> pix_manager_;
};

INSTANTIATE_TEST_SUITE_P(,
                         FacilitatedPaymentsDriverTest,
                         testing::Values(false, true));

TEST_P(FacilitatedPaymentsDriverTest,
       PixIdentifierExists_OnPixCodeCopiedToClipboardTriggered) {
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);

  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard);

  // "0014br.gov.bcb.pix" is the Pix identifier.
  driver_->OnTextCopiedToClipboard(
      /*main_frame_url=*/url,
      /*iframe_url=*/std::nullopt,
      /*main_frame_origin=*/origin, /*copied_text=*/
      u"00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      /*ukm_source_id=*/123);
}

TEST_P(FacilitatedPaymentsDriverTest,
       PixIdentifierAbsent_OnPixCodeCopiedToClipboardNotTriggered) {
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);

  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard).Times(0);

  driver_->OnTextCopiedToClipboard(
      /*main_frame_url=*/url, /*iframe_url=*/std::nullopt,
      /*main_frame_origin=*/origin, /*copied_text=*/u"notAValidPixIdentifier",
      /*ukm_source_id=*/123);
}

}  // namespace

}  // namespace payments::facilitated
