// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace payments {

class PaymentsUtilTest : public testing::Test {
 public:
  PaymentsUtilTest() {}
  ~PaymentsUtilTest() override {}

 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterDoublePref(
        prefs::kAutofillBillingCustomerNumber, 0.0);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestPersonalDataManager personal_data_manager_;
  TestingPrefServiceSimple pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsUtilTest);
};

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Normal) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUsePaymentsCustomerData);
  base::HistogramTester histogram_tester;

  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_EQ(123456,
            GetBillingCustomerId(&personal_data_manager_, &pref_service_,
                                 /*should_log_validity=*/true));

  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::VALID, 1);
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Garbage) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUsePaymentsCustomerData);
  base::HistogramTester histogram_tester;

  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"garbage"));

  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_, &pref_service_,
                                    /*should_log_validity=*/true));

  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::PARSE_ERROR, 1);
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_NoData) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUsePaymentsCustomerData);
  base::HistogramTester histogram_tester;

  // Explictly do not set PaymentsCustomerData. Nothing crashes and the returned
  // customer ID is 0.
  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_, &pref_service_,
                                    /*should_log_validity=*/true));
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::MISSING, 1);
}

TEST_F(PaymentsUtilTest,
       GetBillingCustomerId_PaymentsCustomerData_NoDataFallback) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUsePaymentsCustomerData);
  base::HistogramTester histogram_tester;

  // Explictly do not set PaymentsCustomerData but set a fallback to prefs.
  pref_service_.SetDouble(prefs::kAutofillBillingCustomerNumber, 123456.0);

  // We got the data from prefs and log that the PaymentsCustomerData is
  // invalid.
  EXPECT_EQ(123456,
            GetBillingCustomerId(&personal_data_manager_, &pref_service_,
                                 /*should_log_validity=*/true));
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::MISSING, 1);
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PriorityPrefs_Normal) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillUsePaymentsCustomerData);

  pref_service_.SetDouble(prefs::kAutofillBillingCustomerNumber, 123456.0);

  EXPECT_EQ(123456,
            GetBillingCustomerId(&personal_data_manager_, &pref_service_,
                                 /*should_log_validity=*/true));
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PriorityPrefs_NoData) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillUsePaymentsCustomerData);

  // Explictly do not set Prefs data. Nothing crashes and the returned customer
  // ID is 0.
  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_, &pref_service_,
                                    /*should_log_validity=*/true));
}

}  // namespace payments
}  // namespace autofill
