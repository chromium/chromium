// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/label_formatter.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/ui/address_contact_form_label_formatter.h"
#include "components/autofill/core/browser/ui/address_email_form_label_formatter.h"
#include "components/autofill/core/browser/ui/address_form_label_formatter.h"
#include "components/autofill/core/browser/ui/address_phone_form_label_formatter.h"
#include "components/autofill/core/browser/ui/contact_form_label_formatter.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/browser/ui/mobile_label_formatter.h"

namespace autofill {

using data_util::ContainsAddress;
using data_util::ContainsEmail;
using data_util::ContainsName;
using data_util::ContainsPhone;
using data_util::bit_field_type_groups::kAddress;
using data_util::bit_field_type_groups::kEmail;
using data_util::bit_field_type_groups::kName;
using data_util::bit_field_type_groups::kPhone;

LabelFormatter::LabelFormatter(const std::vector<AutofillProfile*>& profiles,
                               const std::string& app_locale,
                               ServerFieldType focused_field_type,
                               uint32_t groups,
                               const std::vector<ServerFieldType>& field_types)
    : profiles_(profiles),
      app_locale_(app_locale),
      focused_field_type_(focused_field_type),
      groups_(groups) {
  const FieldTypeGroup focused_group = GetFocusedNonBillingGroup();
  std::set<FieldTypeGroup> groups_for_labels{NAME, ADDRESS_HOME, EMAIL,
                                             PHONE_HOME};

  // If a user is focused on an address field, then parts of the address may be
  // shown in the label. For example, if the user is focusing on a street
  // address field, then it may be helpful to show the city in the label.
  // Otherwise, the focused field should not appear in the label.
  if (focused_group != ADDRESS_HOME) {
    groups_for_labels.erase(focused_group);
  }

  // Countries are excluded to prevent them from appearing in labels with
  // national addresses.
  auto can_be_shown_in_label =
      [&groups_for_labels](ServerFieldType type) -> bool {
    return groups_for_labels.find(
               AutofillType(AutofillType(type).GetStorableType()).group()) !=
               groups_for_labels.end() &&
           type != ADDRESS_HOME_COUNTRY && type != ADDRESS_BILLING_COUNTRY;
  };

  std::copy_if(field_types.begin(), field_types.end(),
               std::back_inserter(field_types_for_labels_),
               can_be_shown_in_label);
}

LabelFormatter::~LabelFormatter() = default;

std::vector<base::string16> LabelFormatter::GetLabels() const {
  std::vector<base::string16> labels;
  for (const AutofillProfile* profile : profiles_) {
    labels.push_back(GetLabelForProfile(*profile, GetFocusedNonBillingGroup()));
  }
  return labels;
}

FieldTypeGroup LabelFormatter::GetFocusedNonBillingGroup() const {
  return AutofillType(AutofillType(focused_field_type_).GetStorableType())
      .group();
}

// static
std::unique_ptr<LabelFormatter> LabelFormatter::Create(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types) {
  const uint32_t groups = data_util::DetermineGroups(field_types);
  if (!data_util::IsSupportedFormType(groups)) {
    return nullptr;
  }

#if defined(OS_ANDROID) || defined(OS_IOS)
  return std::make_unique<MobileLabelFormatter>(
      profiles, app_locale, focused_field_type, groups, field_types);
#else
  switch (groups) {
    case kAddress | kEmail | kPhone:
    case kName | kAddress | kEmail | kPhone:
      return std::make_unique<AddressContactFormLabelFormatter>(
          profiles, app_locale, focused_field_type, groups, field_types);
    case kAddress | kPhone:
    case kName | kAddress | kPhone:
      return std::make_unique<AddressPhoneFormLabelFormatter>(
          profiles, app_locale, focused_field_type, groups, field_types);
    case kAddress | kEmail:
    case kName | kAddress | kEmail:
      return std::make_unique<AddressEmailFormLabelFormatter>(
          profiles, app_locale, focused_field_type, groups, field_types);
    case kAddress:
    case kName | kAddress:
      return std::make_unique<AddressFormLabelFormatter>(
          profiles, app_locale, focused_field_type, groups, field_types);
    case kEmail | kPhone:
    case kName | kEmail | kPhone:
    case kName | kEmail:
    case kName | kPhone:
      return std::make_unique<ContactFormLabelFormatter>(
          profiles, app_locale, focused_field_type, groups, field_types);
    default:
      return nullptr;
  }
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
}

}  // namespace autofill
