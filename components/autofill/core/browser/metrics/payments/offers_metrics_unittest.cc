// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Bucket;
using ::base::BucketsAre;

namespace autofill::autofill_metrics {

class OffersMetricsTest : public metrics::AutofillMetricsBaseTest {
 public:
  OffersMetricsTest() = default;
  ~OffersMetricsTest() override = default;
};

TEST_F(OffersMetricsTest, LogStoredOfferMetrics) {
  std::vector<std::unique_ptr<AutofillOfferData>> offers;
  AutofillOfferData offer1 = test::GetCardLinkedOfferData1();
  AutofillOfferData offer2 = test::GetCardLinkedOfferData2();
  AutofillOfferData offer3 = test::GetPromoCodeOfferData();
  offer2.SetEligibleInstrumentIdForTesting({222222, 999999, 888888});
  offer2.SetMerchantOriginForTesting(
      {GURL("http://www.example2.com"), GURL("https://www.example3.com/")});
  offers.push_back(std::make_unique<AutofillOfferData>(offer1));
  offers.push_back(std::make_unique<AutofillOfferData>(offer2));
  offers.push_back(std::make_unique<AutofillOfferData>(offer3));

  base::HistogramTester histogram_tester;

  autofill_metrics::LogStoredOfferMetrics(offers);

  auto SamplesOf = [&histogram_tester](base::StringPiece metric) {
    return histogram_tester.GetAllSamples(metric);
  };

  // Validate the count metrics.
  EXPECT_THAT(SamplesOf("Autofill.Offer.StoredOfferCount.CardLinkedOffer"),
              BucketsAre(Bucket(2, 1)));
  EXPECT_THAT(SamplesOf("Autofill.Offer.StoredOfferCount.GPayPromoCodeOffer"),
              BucketsAre(Bucket(1, 1)));
  EXPECT_THAT(SamplesOf("Autofill.Offer.StoredOfferRelatedMerchantCount"),
              BucketsAre(Bucket(1, 1), Bucket(2, 1)));
  EXPECT_THAT(SamplesOf("Autofill.Offer.StoredOfferRelatedCardCount"),
              BucketsAre(Bucket(1, 1), Bucket(3, 1)));
}

}  // namespace autofill::autofill_metrics
