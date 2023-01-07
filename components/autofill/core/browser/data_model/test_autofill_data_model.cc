// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/test_autofill_data_model.h"

namespace autofill {

TestAutofillDataModel::TestAutofillDataModel(const std::string& guid,
                                             const std::string& origin)
    : AutofillDataModel(guid, origin) {}

TestAutofillDataModel::TestAutofillDataModel(const std::string& guid,
                                             size_t use_count,
                                             base::Time use_date)
    : AutofillDataModel(guid, std::string()) {
  set_use_count(use_count);
  set_use_date(use_date);
}

TestAutofillDataModel::~TestAutofillDataModel() = default;

std::u16string TestAutofillDataModel::GetRawInfo(ServerFieldType type) const {
  return std::u16string();
}

void TestAutofillDataModel::SetRawInfoWithVerificationStatus(
    ServerFieldType type,
    const std::u16string& value,
    structured_address::VerificationStatus status) {}

void TestAutofillDataModel::GetSupportedTypes(
    ServerFieldTypeSet* supported_types) const {}

}  // namespace autofill
