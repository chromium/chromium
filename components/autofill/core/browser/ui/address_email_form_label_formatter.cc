// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_email_form_label_formatter.h"

#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"

namespace autofill {

AddressEmailFormLabelFormatter::AddressEmailFormLabelFormatter(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    uint32_t groups,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(profiles,
                     app_locale,
                     focused_field_type,
                     groups,
                     field_types),
      form_has_street_address_(HasStreetAddress(field_types_for_labels())) {}

AddressEmailFormLabelFormatter::~AddressEmailFormLabelFormatter() {}

std::u16string AddressEmailFormLabelFormatter::GetLabelForProfile(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  return focused_group == FieldTypeGroup::kAddressHome &&
                 !IsStreetAddressPart(focused_field_type())
             ? GetLabelForProfileOnFocusedNonStreetAddress(
                   form_has_street_address_, profile, app_locale(),
                   field_types_for_labels(),
                   GetLabelEmail(profile, app_locale()))
             : GetLabelForProfileOnFocusedNameEmailOrStreetAddress(
                   profile, focused_group);
}

// Note that the order--name, address, and email--in which parts of the label
// are added ensures that the label is formatted correctly for |focused_group|,
// |focused_field_type_| and for this kind of formatter.
std::u16string AddressEmailFormLabelFormatter::
    GetLabelForProfileOnFocusedNameEmailOrStreetAddress(
        const AutofillProfile& profile,
        FieldTypeGroup focused_group) const {
  std::vector<std::u16string> label_parts;

  if (focused_group != FieldTypeGroup::kName &&
      data_util::ContainsName(groups())) {
    AddLabelPartIfNotEmpty(
        GetLabelName(field_types_for_labels(), profile, app_locale()),
        &label_parts);
  }

  if (focused_group != FieldTypeGroup::kAddressHome) {
    AddLabelPartIfNotEmpty(
        GetLabelAddress(form_has_street_address_, profile, app_locale(),
                        field_types_for_labels()),
        &label_parts);
  }

  if (focused_group != FieldTypeGroup::kEmail) {
    AddLabelPartIfNotEmpty(GetLabelEmail(profile, app_locale()), &label_parts);
  }

  return ConstructLabelLine(label_parts);
}

}  // namespace autofill
