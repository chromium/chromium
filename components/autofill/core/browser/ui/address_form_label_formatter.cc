// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_form_label_formatter.h"

#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"

namespace autofill {

AddressFormLabelFormatter::AddressFormLabelFormatter(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    uint32_t groups,
    const ServerFieldTypeSet& field_types)
    : LabelFormatter(profiles,
                     app_locale,
                     focused_field_type,
                     groups,
                     field_types),
      form_has_street_address_(HasStreetAddress(field_types_for_labels())) {}

AddressFormLabelFormatter::~AddressFormLabelFormatter() {}

std::u16string AddressFormLabelFormatter::GetLabelForProfile(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  if (focused_group != FieldTypeGroup::kAddressHome) {
    return GetLabelNationalAddress(field_types_for_labels(), profile,
                                   app_locale());
  } else {
    std::vector<std::u16string> label_parts;

    if (data_util::ContainsName(groups())) {
      AddLabelPartIfNotEmpty(
          GetLabelName(field_types_for_labels(), profile, app_locale()),
          &label_parts);
    }

    AddLabelPartIfNotEmpty(GetLabelForFocusedAddress(
                               focused_field_type(), form_has_street_address_,
                               profile, app_locale(), field_types_for_labels()),
                           &label_parts);
    return ConstructLabelLine(label_parts);
  }
}

}  // namespace autofill
