// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/label_formatter.h"

#include <iterator>
#include <set>

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/address_contact_form_label_formatter.h"
#include "components/autofill/core/browser/ui/address_email_form_label_formatter.h"
#include "components/autofill/core/browser/ui/address_form_label_formatter.h"
#include "components/autofill/core/browser/ui/address_phone_form_label_formatter.h"
#include "components/autofill/core/browser/ui/contact_form_label_formatter.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/browser/ui/mobile_label_formatter.h"
#include "components/autofill/core/common/dense_set.h"

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
  const FieldTypeGroup focused_group = AutofillType(focused_field_type).group();
  DenseSet<FieldTypeGroup> groups_for_labels{
      FieldTypeGroup::kName, FieldTypeGroup::kAddressHome,
      FieldTypeGroup::kEmail, FieldTypeGroup::kPhoneHome};

  // If a user is focused on an address field, then parts of the address may be
  // shown in the label. For example, if the user is focusing on a street
  // address field, then it may be helpful to show the city in the label.
  // Otherwise, the focused field should not appear in the label.
  if (focused_group != FieldTypeGroup::kAddressHome) {
    groups_for_labels.erase(focused_group);
  }

  // Countries are excluded to prevent them from appearing in labels with
  // national addresses.
  auto can_be_shown_in_label =
      [&groups_for_labels](ServerFieldType type) -> bool {
    return groups_for_labels.find(
               AutofillType(AutofillType(type).GetStorableType()).group()) !=
               groups_for_labels.end() &&
           type != ADDRESS_HOME_COUNTRY;
  };

  base::ranges::copy_if(field_types,
                        std::back_inserter(field_types_for_labels_),
                        can_be_shown_in_label);
}

LabelFormatter::~LabelFormatter() = default;

std::vector<std::u16string> LabelFormatter::GetLabels() const {
  std::vector<std::u16string> labels;
  for (const AutofillProfile* profile : *profiles_) {
    labels.push_back(GetLabelForProfile(
        *profile, AutofillType(focused_field_type_).group()));
  }
  return labels;
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

}  // namespace autofill
