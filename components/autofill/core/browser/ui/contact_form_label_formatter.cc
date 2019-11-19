// Copyright 2019 The Chromium Authors. All rights reserved.
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
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(profiles,
                     app_locale,
                     focused_field_type,
                     groups,
                     field_types) {}

ContactFormLabelFormatter::~ContactFormLabelFormatter() {}

// Note that the order--name, phone, and email--in which parts of the label
// are possibly added ensures that the label is formatted correctly for
// |focused_group| and for this kind of formatter.
base::string16 ContactFormLabelFormatter::GetLabelForProfile(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  std::vector<base::string16> label_parts;
  if (focused_group != NAME && data_util::ContainsName(groups())) {
    AddLabelPartIfNotEmpty(
        GetLabelName(field_types_for_labels(), profile, app_locale()),
        &label_parts);
  }

  if (focused_group != PHONE_HOME) {
    AddLabelPartIfNotEmpty(MaybeGetPhone(profile), &label_parts);
  }

  if (focused_group != EMAIL) {
    AddLabelPartIfNotEmpty(MaybeGetEmail(profile), &label_parts);
  }

  return ConstructLabelLine(label_parts);
}

base::string16 ContactFormLabelFormatter::MaybeGetEmail(
    const AutofillProfile& profile) const {
  return data_util::ContainsEmail(groups())
             ? GetLabelEmail(profile, app_locale())
             : base::string16();
}

base::string16 ContactFormLabelFormatter::MaybeGetPhone(
    const AutofillProfile& profile) const {
  return data_util::ContainsPhone(groups())
             ? GetLabelPhone(profile, app_locale())
             : base::string16();
}

}  // namespace autofill
