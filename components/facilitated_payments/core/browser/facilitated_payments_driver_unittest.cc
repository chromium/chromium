// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
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

struct IframeUrlTypeTestCase {
  std::string iframe_url;
  PixIframeUrlType expected_type;
};

struct IframeIsSameOriginTestCase {
  std::string iframe_url;
  bool is_same_origin;
  bool should_log;
};

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

class FacilitatedPaymentsDriverTestBase : public testing::Test {
 public:
  FacilitatedPaymentsDriverTestBase() {
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

  explicit FacilitatedPaymentsDriverTestBase(bool use_rust_validator)
      : FacilitatedPaymentsDriverTestBase() {
    scoped_feature_list_.InitWithFeatureState(kUseRustPixCodeValidator,
                                              use_rust_validator);
  }

  ~FacilitatedPaymentsDriverTestBase() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::TestOptimizationGuideDecider decider_;
  MockFacilitatedPaymentsClient client_;
  std::unique_ptr<FacilitatedPaymentsDriver> driver_;
  raw_ptr<MockPixManager> pix_manager_;
};

class FacilitatedPaymentsDriverTest : public FacilitatedPaymentsDriverTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  FacilitatedPaymentsDriverTest()
      : FacilitatedPaymentsDriverTestBase(GetParam()) {}
};

class FacilitatedPaymentsDriverIframeUrlTypeTest
    : public FacilitatedPaymentsDriverTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, IframeUrlTypeTestCase>> {
 public:
  FacilitatedPaymentsDriverIframeUrlTypeTest()
      : FacilitatedPaymentsDriverTestBase(std::get<0>(GetParam())) {}
};

class FacilitatedPaymentsDriverIframeIsSameOriginTest
    : public FacilitatedPaymentsDriverTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, IframeIsSameOriginTestCase>> {
 public:
  FacilitatedPaymentsDriverIframeIsSameOriginTest()
      : FacilitatedPaymentsDriverTestBase(std::get<0>(GetParam())) {}
};

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
      /*ukm_source_id=*/123,
      /*is_same_origin=*/false);
}

TEST_P(FacilitatedPaymentsDriverTest,
       PixIdentifierAbsent_OnPixCodeCopiedToClipboardNotTriggered) {
  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);

  EXPECT_CALL(*pix_manager_, OnPixCodeCopiedToClipboard).Times(0);

  driver_->OnTextCopiedToClipboard(
      /*main_frame_url=*/url, /*iframe_url=*/std::nullopt,
      /*main_frame_origin=*/origin, /*copied_text=*/u"notAValidPixIdentifier",
      /*ukm_source_id=*/123,
      /*is_same_origin=*/false);
}

TEST_P(FacilitatedPaymentsDriverIframeUrlTypeTest, UrlTypeLogged) {
  const IframeUrlTypeTestCase& test_case = std::get<1>(GetParam());

  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);
  const std::u16string kValidPixCode =
      u"00020126370014br.gov.bcb.pix2515www.example.com6304EA3F";

  base::HistogramTester histogram_tester;

  driver_->OnTextCopiedToClipboard(
      /*main_frame_url=*/url,
      /*iframe_url=*/std::make_optional(GURL(test_case.iframe_url)),
      /*main_frame_origin=*/origin, /*copied_text=*/kValidPixCode,
      /*ukm_source_id=*/123,
      /*is_same_origin=*/
      url::Origin::Create(GURL(test_case.iframe_url)).IsSameOriginWith(origin));

  histogram_tester.ExpectUniqueSample("FacilitatedPayments.Pix.Iframe.UrlType",
                                      /*sample=*/test_case.expected_type,
                                      /*expected_bucket_count=*/1);
}

TEST_P(FacilitatedPaymentsDriverIframeIsSameOriginTest, IsSameOriginLogged) {
  const IframeIsSameOriginTestCase& test_case = std::get<1>(GetParam());

  GURL url("https://example.com/");
  url::Origin origin = url::Origin::Create(url);
  const std::u16string kValidPixCode =
      u"00020126370014br.gov.bcb.pix2515www.example.com6304EA3F";

  base::HistogramTester histogram_tester;

  driver_->OnTextCopiedToClipboard(
      /*main_frame_url=*/url,
      /*iframe_url=*/std::make_optional(GURL(test_case.iframe_url)),
      /*main_frame_origin=*/origin, /*copied_text=*/kValidPixCode,
      /*ukm_source_id=*/123,
      /*is_same_origin=*/test_case.is_same_origin);

  if (test_case.should_log) {
    histogram_tester.ExpectUniqueSample(
        "FacilitatedPayments.Pix.Iframe.IsSameOrigin",
        /*sample=*/test_case.is_same_origin,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "FacilitatedPayments.Pix.Iframe.IsSameOrigin", 0);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         FacilitatedPaymentsDriverTest,
                         testing::Values(false, true));

INSTANTIATE_TEST_SUITE_P(
    All,
    FacilitatedPaymentsDriverIframeUrlTypeTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(
            IframeUrlTypeTestCase{"https://psp.com",
                                  PixIframeUrlType::kOtherNonEmptyUrl},
            IframeUrlTypeTestCase{"about:blank", PixIframeUrlType::kAboutBlank},
            IframeUrlTypeTestCase{"", PixIframeUrlType::kEmpty},
            IframeUrlTypeTestCase{"about:srcdoc",
                                  PixIframeUrlType::kAboutSrcDoc},
            IframeUrlTypeTestCase{
                "https://example.com/",
                PixIframeUrlType::kNonEmptyAndSameOriginAsMainFrame})));

INSTANTIATE_TEST_SUITE_P(
    All,
    FacilitatedPaymentsDriverIframeIsSameOriginTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(
            // For empty, about:blank, about:srcdoc, we log the is_same_origin
            // value.
            IframeIsSameOriginTestCase{"about:blank", true, true},
            IframeIsSameOriginTestCase{"about:blank", false, true},
            IframeIsSameOriginTestCase{"", true, true},
            IframeIsSameOriginTestCase{"", false, true},
            IframeIsSameOriginTestCase{"about:srcdoc", true, true},
            IframeIsSameOriginTestCase{"about:srcdoc", false, true},
            // For standard URLs, we do not log the IsSameOrigin metric.
            IframeIsSameOriginTestCase{"https://example.com/", true, false},
            IframeIsSameOriginTestCase{"https://example.com/", false, false},
            IframeIsSameOriginTestCase{"https://psp.com/", true, false},
            IframeIsSameOriginTestCase{"https://psp.com/", false, false})));
}  // namespace

}  // namespace payments::facilitated
