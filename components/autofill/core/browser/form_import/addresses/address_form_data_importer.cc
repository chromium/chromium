// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"

namespace autofill {

AddressFormDataImporter::AddressFormDataImporter(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

AddressFormDataImporter::~AddressFormDataImporter() = default;

void AddressFormDataImporter::RemoveInaccessibleProfileValues(
    AutofillProfile& profile) {
  const FieldTypeSet inaccessible_fields =
      profile.FindInaccessibleProfileValues();
  profile.ClearFields(inaccessible_fields);
  autofill_metrics::LogRemovedSettingInaccessibleFields(
      !inaccessible_fields.empty());
  for (const FieldType inaccessible_field : inaccessible_fields) {
    autofill_metrics::LogRemovedSettingInaccessibleField(inaccessible_field);
  }
}

AddressDataManager& AddressFormDataImporter::address_data_manager() {
  return client_->GetPersonalDataManager().address_data_manager();
}

}  // namespace autofill
