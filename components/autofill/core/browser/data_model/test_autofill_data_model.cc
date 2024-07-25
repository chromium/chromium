// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/test_autofill_data_model.h"

namespace autofill {

TestAutofillDataModel::TestAutofillDataModel(size_t usage_history_size)
    : AutofillDataModel(usage_history_size) {}

TestAutofillDataModel::TestAutofillDataModel(size_t use_count,
                                             base::Time use_date) {
  set_use_count(use_count);
  set_use_date(use_date);
}

TestAutofillDataModel::~TestAutofillDataModel() = default;

std::u16string TestAutofillDataModel::GetRawInfo(FieldType type) const {
  return std::u16string();
}

void TestAutofillDataModel::SetRawInfoWithVerificationStatus(
    FieldType type,
    const std::u16string& value,
    VerificationStatus status) {}

void TestAutofillDataModel::GetSupportedTypes(
    FieldTypeSet* supported_types) const {}

}  // namespace autofill
