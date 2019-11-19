// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/mobile_label_formatter.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

using data_util::ContainsAddress;
using data_util::ContainsEmail;
using data_util::ContainsName;
using data_util::ContainsPhone;
using data_util::bit_field_type_groups::kAddress;
using data_util::bit_field_type_groups::kEmail;
using data_util::bit_field_type_groups::kName;
using data_util::bit_field_type_groups::kPhone;

MobileLabelFormatter::MobileLabelFormatter(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    uint32_t groups,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(profiles,
                     app_locale,
                     focused_field_type,
                     groups,
                     field_types) {
  const FieldTypeGroup focused_group = GetFocusedNonBillingGroup();

  could_show_email_ = HasUnfocusedEmailField(focused_group, groups) &&
                      !HaveSameEmailAddresses(profiles, app_locale);

  could_show_name_ = HasUnfocusedNameField(focused_group, groups) &&
                     !HaveSameFirstNames(profiles, app_locale);

  // Non street address elements, e.g. Mountain View or 28199 Bremen, can only
  // only be included in labels when the form's |field_types| do not have a
  // street address field.
  could_show_non_street_address_ =
      !HasStreetAddress(field_types_for_labels()) &&
      HasUnfocusedNonStreetAddressField(focused_field_type, focused_group,
                                        field_types_for_labels()) &&
      !HaveSameNonStreetAddresses(profiles, app_locale,
                                  ExtractSpecifiedAddressFieldTypes(
                                      /*extract_street_address_types=*/false,
                                      field_types_for_labels()));

  could_show_phone_ = HasUnfocusedPhoneField(focused_group, groups) &&
                      !HaveSamePhoneNumbers(profiles, app_locale);

  could_show_street_address_ =
      HasUnfocusedStreetAddressField(focused_field_type, focused_group,
                                     field_types_for_labels()) &&
      !HaveSameStreetAddresses(
          profiles, app_locale,
          ExtractSpecifiedAddressFieldTypes(
              /*extract_street_address_types=*/true, field_types_for_labels()));
}

MobileLabelFormatter::~MobileLabelFormatter() = default;

base::string16 MobileLabelFormatter::GetLabelForProfile(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  std::string label_variant = base::GetFieldTrialParamValueByFeature(
      features::kAutofillUseMobileLabelDisambiguation,
      features::kAutofillUseMobileLabelDisambiguationParameterName);
  if (label_variant ==
      features::kAutofillUseMobileLabelDisambiguationParameterShowOne) {
    return GetLabelForShowOneVariant(profile, focused_group);
  } else if (label_variant ==
             features::kAutofillUseMobileLabelDisambiguationParameterShowAll) {
    return GetLabelForShowAllVariant(profile, focused_group);
  }
  // An unknown parameter was received.
  NOTREACHED();
  return base::string16();
}

// The order in which pieces of data are considered--address, and then, if the
// data is not the same across |profiles_|, phone number, email address, and
// name--ensures that the label contains the most useful information given the
// |focused_group| and the |focused_field_type_|.
base::string16 MobileLabelFormatter::GetLabelForShowOneVariant(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  if (ShowLabelAddress(focused_group)) {
    return GetLabelAddress(
        /*use_street_address=*/HasStreetAddress(field_types_for_labels()),
        profile, app_locale(),
        TypesWithoutFocusedField(field_types_for_labels(),
                                 focused_field_type()));
  }

  // If an unfocused form field does not have the same data for all
  // |profiles_|, then it is preferable to show this data in the label.
  if (could_show_phone_) {
    return GetLabelPhone(profile, app_locale());
  }

  if (could_show_email_) {
    return GetLabelEmail(profile, app_locale());
  }

  if (could_show_name_) {
    return GetLabelName(field_types_for_labels(), profile, app_locale());
  }

  // In the case that data for unfocused form fields is the same for all
  // |profiles_|, a label with the most important data is returned.
  return GetDefaultLabel(profile, focused_group);
}

// The order in which parts of a label are added--name, address, phone number,
// and email address--ensures that the label is formatted correctly for the
// |focused_group|, the |focused_field_type_|, and the non-focused form fields
// whose data is not the same across |profiles_|.
base::string16 MobileLabelFormatter::GetLabelForShowAllVariant(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  if (!(could_show_email_ || could_show_name_ ||
        could_show_non_street_address_ || could_show_phone_ ||
        could_show_street_address_)) {
    // If there is profile data corresponding to unfocused form fields that is
    // not the same across all |profiles_|, then include this data in the label.
    // Otherwise, show the most important piece of data.
    return GetDefaultLabel(profile, focused_group);
  }

  std::vector<base::string16> label_parts;

  // TODO(crbug.com/961819): Maybe put name after address for some app locales.
  if (could_show_name_) {
    // Due to mobile platforms' space constraints, only the first name is shown
    // if the form contains a first name field or a full name field.
    std::any_of(field_types_for_labels().begin(),
                field_types_for_labels().end(),
                [](ServerFieldType type) {
                  return type == NAME_FIRST || type == NAME_FULL;
                })
        ? AddLabelPartIfNotEmpty(GetLabelFirstName(profile, app_locale()),
                                 &label_parts)
        : AddLabelPartIfNotEmpty(
              GetLabelName(field_types_for_labels(), profile, app_locale()),
              &label_parts);
  }

  if (could_show_street_address_) {
    AddLabelPartIfNotEmpty(
        GetLabelAddress(/*use_street_address=*/true, profile, app_locale(),
                        field_types_for_labels()),
        &label_parts);
  }

  if (could_show_non_street_address_) {
    AddLabelPartIfNotEmpty(
        GetLabelAddress(/*use_street_address=*/false, profile, app_locale(),
                        field_types_for_labels()),
        &label_parts);
  }

  if (could_show_phone_) {
    AddLabelPartIfNotEmpty(GetLabelPhone(profile, app_locale()), &label_parts);
  }

  if (could_show_email_) {
    AddLabelPartIfNotEmpty(GetLabelEmail(profile, app_locale()), &label_parts);
  }

  return ConstructMobileLabelLine(label_parts);
}

// The order in which pieces of data are considered--address, phone number,
// email address, and name--ensures that the label contains the most useful
// information given the |focused_group| and the |focused_field_type_|.
base::string16 MobileLabelFormatter::GetDefaultLabel(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  if (ShowLabelAddress(focused_group)) {
    return GetLabelAddress(
        /*use_street_address=*/HasStreetAddress(field_types_for_labels()),
        profile, app_locale(),
        TypesWithoutFocusedField(field_types_for_labels(),
                                 focused_field_type()));
  }

  if (HasUnfocusedPhoneField(focused_group, groups())) {
    return GetLabelPhone(profile, app_locale());
  }

  if (HasUnfocusedEmailField(focused_group, groups())) {
    return GetLabelEmail(profile, app_locale());
  }

  return GetLabelName(field_types_for_labels(), profile, app_locale());
}

bool MobileLabelFormatter::ShowLabelAddress(
    FieldTypeGroup focused_group) const {
  if (HasUnfocusedStreetAddressField(focused_field_type(), focused_group,
                                     field_types_for_labels())) {
    return true;
  }

  // If a form lacks a street address field, then a non street address field may
  // be shown. It is shown in two situations:
  // 1. A form has an unfocused non street address field.
  // 2. A form has only non street address fields.
  return (!HasStreetAddress(field_types_for_labels()) &&
          HasUnfocusedNonStreetAddressField(focused_field_type(), focused_group,
                                            field_types_for_labels())) ||
         FormHasOnlyNonStreetAddressFields(field_types_for_labels(), groups());
}

}  // namespace autofill
