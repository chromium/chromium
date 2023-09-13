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
  explicit TestAutofillDataModel();
  TestAutofillDataModel(size_t use_count, base::Time use_date);

  TestAutofillDataModel(const TestAutofillDataModel&) = delete;
  TestAutofillDataModel& operator=(const TestAutofillDataModel&) = delete;

  ~TestAutofillDataModel() override;

 private:
  std::u16string GetRawInfo(ServerFieldType type) const override;
  void SetRawInfoWithVerificationStatus(ServerFieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TEST_AUTOFILL_DATA_MODEL_H_
