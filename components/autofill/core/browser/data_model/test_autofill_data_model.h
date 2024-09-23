// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TEST_AUTOFILL_DATA_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TEST_AUTOFILL_DATA_MODEL_H_

#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class TestAutofillDataModel : public AutofillDataModel {
 public:
  explicit TestAutofillDataModel(size_t usage_history_size = 1);
  TestAutofillDataModel(size_t use_count, base::Time use_date);

  TestAutofillDataModel(const TestAutofillDataModel&) = default;
  TestAutofillDataModel& operator=(const TestAutofillDataModel&) = default;

  ~TestAutofillDataModel() override;

  // For easier access during testing.
  using AutofillDataModel::MergeUseDates;

 private:
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  void GetSupportedTypes(FieldTypeSet* supported_types) const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TEST_AUTOFILL_DATA_MODEL_H_
