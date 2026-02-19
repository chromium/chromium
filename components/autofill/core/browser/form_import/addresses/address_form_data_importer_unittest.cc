// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AddressFormDataImporterTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient> {
 public:
  void SetUp() override { InitAutofillClient(); }

  void TearDown() override { DestroyAutofillClient(); }

  AddressFormDataImporter& GetAddressFormDataImporter() {
    return autofill_client()
        .GetFormDataImporter()
        ->GetAddressFormDataImporter();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AddressFormDataImporterTest,
       GetAddressObservedFieldValues_FiltersPlaceholderValues) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillFilterPlaceholderValuesOnImport);

  AutofillField field1;
  field1.set_value(u"Please select a city");
  field1.SetTypeTo(AutofillType(ADDRESS_HOME_CITY),
                   AutofillPredictionSource::kHeuristics);

  AutofillField field2;
  field2.set_value(u"123 Main St");
  field2.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1),
                   AutofillPredictionSource::kHeuristics);

  std::vector<const AutofillField*> section_fields = {&field1, &field2};
  base::HistogramTester histogram_tester;
  base::flat_map<FieldType, std::u16string> observed_values =
      test_api(GetAddressFormDataImporter())
          .GetObservedFieldValues(section_fields);

  EXPECT_FALSE(observed_values.contains(ADDRESS_HOME_CITY));
  EXPECT_TRUE(observed_values.contains(ADDRESS_HOME_LINE1));

  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileImport.PlaceholderValueRemoved.ByFieldType",
      ADDRESS_HOME_CITY, 1);
}

}  // namespace autofill
