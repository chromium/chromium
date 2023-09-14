// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Bucket;
using ::base::BucketsAre;

namespace autofill::autofill_metrics {

class ConvergeToExtremeLengthAddressMetricsTest
    : public autofill_metrics::AutofillMetricsBaseTest,
      public testing::Test {
 public:
  void SetUp() override {
    SetUpHelper();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillConvergeToExtremeLengthStreetAddress,
        {{features::kAutofillConvergeToLonger.name, "true" /*longer*/}});

    old_address_ = i18n_model_definition::CreateAddressComponentModel();
    new_address_ = i18n_model_definition::CreateAddressComponentModel();
    old_street_ =
        old_address_->GetNodeForTypeForTesting(ADDRESS_HOME_STREET_ADDRESS);
    new_street_ =
        new_address_->GetNodeForTypeForTesting(ADDRESS_HOME_STREET_ADDRESS);
  }
  void TearDown() override { TearDownHelper(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AddressComponent> old_address_;
  std::unique_ptr<AddressComponent> new_address_;
  raw_ptr<AddressComponent> old_street_ = nullptr;
  raw_ptr<AddressComponent> new_street_ = nullptr;
};

// Tests the logging of preferring old street address value during merging, when
// the feature `kAutofillConvergeToExtremeLengthStreetAddress` is enabled.
TEST_F(ConvergeToExtremeLengthAddressMetricsTest, LogPreferringOldValue) {
  old_street_->SetValue(u"Wall Street", VerificationStatus::kParsed);
  new_street_->SetValue(u"Wall St", VerificationStatus::kParsed);

  base::HistogramTester histogram_tester;
  old_street_->MergeWithComponent(*new_street_);

  histogram_tester.ExpectUniqueSample(
      "Autofill.NewerStreetAddressWithSameStatusIsChosen", false, 1);
}

// Tests the logging of preferring new street address value during merging, when
// the feature `kAutofillConvergeToExtremeLengthStreetAddress` is enabled.
TEST_F(ConvergeToExtremeLengthAddressMetricsTest, LogPreferringNewValue) {
  old_street_->SetValue(u"Wall St", VerificationStatus::kParsed);
  new_street_->SetValue(u"Wall Street", VerificationStatus::kParsed);

  base::HistogramTester histogram_tester;
  old_street_->MergeWithComponent(*new_street_);

  histogram_tester.ExpectUniqueSample(
      "Autofill.NewerStreetAddressWithSameStatusIsChosen", true, 1);
}

}  // namespace autofill::autofill_metrics
