// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/contact_form_label_formatter.h"

#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"

namespace autofill {

ContactFormLabelFormatter::ContactFormLabelFormatter(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    uint32_t groups,
    const ServerFieldTypeSet& field_types)
    : LabelFormatter(profiles,
                     app_locale,
                     focused_field_type,
                     groups,
                     field_types) {}

ContactFormLabelFormatter::~ContactFormLabelFormatter() {}

// Note that the order--name, phone, and email--in which parts of the label
// are possibly added ensures that the label is formatted correctly for
// |focused_group| and for this kind of formatter.
std::u16string ContactFormLabelFormatter::GetLabelForProfile(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  std::vector<std::u16string> label_parts;
  if (focused_group != FieldTypeGroup::kName &&
      data_util::ContainsName(groups())) {
    AddLabelPartIfNotEmpty(
        GetLabelName(field_types_for_labels(), profile, app_locale()),
        &label_parts);
  }

  if (focused_group != FieldTypeGroup::kPhone) {
    AddLabelPartIfNotEmpty(MaybeGetPhone(profile), &label_parts);
  }

  if (focused_group != FieldTypeGroup::kEmail) {
    AddLabelPartIfNotEmpty(MaybeGetEmail(profile), &label_parts);
  }

  return ConstructLabelLine(label_parts);
}

std::u16string ContactFormLabelFormatter::MaybeGetEmail(
    const AutofillProfile& profile) const {
  return data_util::ContainsEmail(groups())
             ? GetLabelEmail(profile, app_locale())
             : std::u16string();
}

std::u16string ContactFormLabelFormatter::MaybeGetPhone(
    const AutofillProfile& profile) const {
  return data_util::ContainsPhone(groups())
             ? GetLabelPhone(profile, app_locale())
             : std::u16string();
}

}  // namespace autofill
