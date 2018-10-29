// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <set>

#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metadata.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile_comparator.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/contact_info.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/state_names.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/translit.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_metadata.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;

namespace autofill {
namespace {

// Like |AutofillType::GetStorableType()|, but also returns |NAME_FULL| for
// first, middle, and last name field types, and groups phone number types
// similarly.
ServerFieldType GetStorableTypeCollapsingGroups(ServerFieldType type) {
  ServerFieldType storable_type = AutofillType(type).GetStorableType();
  if (AutofillType(storable_type).group() == NAME)
    return NAME_FULL;

  if (AutofillType(storable_type).group() == PHONE_HOME)
    return PHONE_HOME_WHOLE_NUMBER;

  return storable_type;
}

// Returns a value that represents specificity/privacy of the given type. This
// is used for prioritizing which data types are shown in inferred labels. For
// example, if the profile is going to fill ADDRESS_HOME_ZIP, it should
// prioritize showing that over ADDRESS_HOME_STATE in the suggestion sublabel.
int SpecificityForType(ServerFieldType type) {
  switch (type) {
    case ADDRESS_HOME_LINE1:
      return 1;

    case ADDRESS_HOME_LINE2:
      return 2;

    case EMAIL_ADDRESS:
      return 3;

    case PHONE_HOME_WHOLE_NUMBER:
      return 4;

    case NAME_FULL:
      return 5;

    case ADDRESS_HOME_ZIP:
      return 6;

    case ADDRESS_HOME_SORTING_CODE:
      return 7;

    case COMPANY_NAME:
      return 8;

    case ADDRESS_HOME_CITY:
      return 9;

    case ADDRESS_HOME_STATE:
      return 10;

    case ADDRESS_HOME_COUNTRY:
      return 11;

    default:
      break;
  }

  // The priority of other types is arbitrary, but deterministic.
  return 100 + type;
}

bool CompareSpecificity(ServerFieldType type1, ServerFieldType type2) {
  return SpecificityForType(type1) < SpecificityForType(type2);
}

// Fills |distinguishing_fields| with a list of fields to use when creating
// labels that can help to distinguish between two profiles. Draws fields from
// |suggested_fields| if it is non-NULL; otherwise returns a default list.
// If |suggested_fields| is non-NULL, does not include |excluded_field| in the
// list. Otherwise, |excluded_field| is ignored, and should be set to
// |UNKNOWN_TYPE| by convention. The resulting list of fields is sorted in
// decreasing order of importance.
void GetFieldsForDistinguishingProfiles(
    const std::vector<ServerFieldType>* suggested_fields,
    ServerFieldType excluded_field,
    std::vector<ServerFieldType>* distinguishing_fields) {
  static const ServerFieldType kDefaultDistinguishingFields[] = {
    NAME_FULL,
    ADDRESS_HOME_LINE1,
    ADDRESS_HOME_LINE2,
    ADDRESS_HOME_DEPENDENT_LOCALITY,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_STATE,
    ADDRESS_HOME_ZIP,
    ADDRESS_HOME_SORTING_CODE,
    ADDRESS_HOME_COUNTRY,
    EMAIL_ADDRESS,
    PHONE_HOME_WHOLE_NUMBER,
    COMPANY_NAME,
  };

  std::vector<ServerFieldType> default_fields;
  if (!suggested_fields) {
    default_fields.assign(
        kDefaultDistinguishingFields,
        kDefaultDistinguishingFields + arraysize(kDefaultDistinguishingFields));
    if (excluded_field == UNKNOWN_TYPE) {
      distinguishing_fields->swap(default_fields);
      return;
    }
    suggested_fields = &default_fields;
  }

  // Keep track of which fields we've seen so that we avoid duplicate entries.
  // Always ignore fields of unknown type and the excluded field.
  std::set<ServerFieldType> seen_fields;
  seen_fields.insert(UNKNOWN_TYPE);
  seen_fields.insert(GetStorableTypeCollapsingGroups(excluded_field));

  distinguishing_fields->clear();
  for (const ServerFieldType& it : *suggested_fields) {
    ServerFieldType suggested_type = GetStorableTypeCollapsingGroups(it);
    if (seen_fields.insert(suggested_type).second)
      distinguishing_fields->push_back(suggested_type);
  }

  std::sort(distinguishing_fields->begin(), distinguishing_fields->end(),
            CompareSpecificity);

  // Special case: If the excluded field is a partial name (e.g. first name) and
  // the suggested fields include other name fields, include |NAME_FULL| in the
  // list of distinguishing fields as a last-ditch fallback. This allows us to
  // distinguish between profiles that are identical except for the name.
  ServerFieldType effective_excluded_type =
      GetStorableTypeCollapsingGroups(excluded_field);
  if (excluded_field != effective_excluded_type) {
    for (const ServerFieldType& it : *suggested_fields) {
      if (it != excluded_field &&
          GetStorableTypeCollapsingGroups(it) == effective_excluded_type) {
        distinguishing_fields->push_back(effective_excluded_type);
        break;
      }
    }
  }
}

// Constants for the validity bitfield.
const size_t kValidityBitsPerType = 2;
// The order is important to ensure a consistent bitfield value. New values
// should be added at the end NOT at the start or middle.
const ServerFieldType kSupportedTypesByClientForValidation[] = {
    ADDRESS_HOME_COUNTRY,
    ADDRESS_HOME_STATE,
    ADDRESS_HOME_ZIP,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_DEPENDENT_LOCALITY,
    EMAIL_ADDRESS,
    PHONE_HOME_WHOLE_NUMBER};

const size_t kNumSupportedTypesForValidation =
    sizeof(kSupportedTypesByClientForValidation) /
    sizeof(kSupportedTypesByClientForValidation[0]);

static_assert(kNumSupportedTypesForValidation * kValidityBitsPerType <= 64,
              "Not enough bits to encode profile validity information!");

// Some types are specializations of other types. Normalize these back to the
// main stored type for used to mark field validity .
ServerFieldType NormalizeTypeForValidityCheck(ServerFieldType type) {
  auto field_type_group = AutofillType(type).group();
  if (field_type_group == PHONE_HOME || field_type_group == PHONE_BILLING)
    return PHONE_HOME_WHOLE_NUMBER;
  return type;
}

}  // namespace

AutofillProfile::AutofillProfile(const std::string& guid,
                                 const std::string& origin)
    : AutofillDataModel(guid, origin),
      phone_number_(this),
      record_type_(LOCAL_PROFILE),
      has_converted_(false) {}

AutofillProfile::AutofillProfile(RecordType type, const std::string& server_id)
    : AutofillDataModel(base::GenerateGUID(), std::string()),
      phone_number_(this),
      server_id_(server_id),
      record_type_(type),
      has_converted_(false) {
  DCHECK(type == SERVER_PROFILE);
}

AutofillProfile::AutofillProfile()
    : AutofillDataModel(base::GenerateGUID(), std::string()),
      phone_number_(this),
      record_type_(LOCAL_PROFILE),
      has_converted_(false) {}

AutofillProfile::AutofillProfile(const AutofillProfile& profile)
    : AutofillDataModel(std::string(), std::string()), phone_number_(this) {
  operator=(profile);
}

AutofillProfile::~AutofillProfile() {
}

AutofillProfile& AutofillProfile::operator=(const AutofillProfile& profile) {
  if (this == &profile)
    return *this;

  set_use_count(profile.use_count());
  set_use_date(profile.use_date());
  set_previous_use_date(profile.previous_use_date());
  set_modification_date(profile.modification_date());

  set_guid(profile.guid());
  set_origin(profile.origin());

  record_type_ = profile.record_type_;

  name_ = profile.name_;
  email_ = profile.email_;
  company_ = profile.company_;
  phone_number_ = profile.phone_number_;
  phone_number_.set_profile(this);

  address_ = profile.address_;
  set_language_code(profile.language_code());

  server_id_ = profile.server_id();
  has_converted_ = profile.has_converted();
  is_client_validity_states_updated_ =
      profile.is_client_validity_states_updated();
  SetClientValidityFromBitfieldValue(profile.GetClientValidityBitfieldValue());
  server_validity_states_ = profile.GetServerValidityMap();

  return *this;
}

AutofillMetadata AutofillProfile::GetMetadata() const {
  AutofillMetadata metadata = AutofillDataModel::GetMetadata();
  metadata.id = (record_type_ == LOCAL_PROFILE ? guid() : server_id_);
  metadata.has_converted = has_converted_;
  return metadata;
}

bool AutofillProfile::SetMetadata(const AutofillMetadata metadata) {
  // Make sure the ids matches.
  if (metadata.id != (record_type_ == LOCAL_PROFILE ? guid() : server_id_))
    return false;

  if (!AutofillDataModel::SetMetadata(metadata))
    return false;

  has_converted_ = metadata.has_converted;
  return true;
}

// TODO(crbug.com/589535): Disambiguate similar field types before uploading.
void AutofillProfile::GetMatchingTypes(
    const base::string16& text,
    const std::string& app_locale,
    ServerFieldTypeSet* matching_types) const {
  ServerFieldTypeSet matching_types_in_this_profile;
  FormGroupList info = FormGroups();
  for (const auto* form_group : info) {
    form_group->GetMatchingTypes(text, app_locale,
                                 &matching_types_in_this_profile);
  }

  for (auto type : matching_types_in_this_profile) {
    matching_types->insert(type);
  }
}

void AutofillProfile::GetMatchingTypesAndValidities(
    const base::string16& text,
    const std::string& app_locale,
    ServerFieldTypeSet* matching_types,
    std::map<ServerFieldType, AutofillProfile::ValidityState>*
        matching_types_validities) const {
  if (!matching_types && !matching_types_validities)
    return;

  ServerFieldTypeSet matching_types_in_this_profile;
  FormGroupList info = FormGroups();
  for (const auto* form_group : info) {
    form_group->GetMatchingTypes(text, app_locale,
                                 &matching_types_in_this_profile);
  }

  for (auto type : matching_types_in_this_profile) {
    if (matching_types_validities) {
      // TODO(crbug.com/879655): Set the client validities and look them up when
      // the server validities are not available.
      (*matching_types_validities)[type] = GetValidityState(type, SERVER);
    }
    if (matching_types)
      matching_types->insert(type);
  }
}

base::string16 AutofillProfile::GetRawInfo(ServerFieldType type) const {
  const FormGroup* form_group = FormGroupForType(AutofillType(type));
  if (!form_group)
    return base::string16();

  return form_group->GetRawInfo(type);
}

void AutofillProfile::SetRawInfo(ServerFieldType type,
                                 const base::string16& value) {
  FormGroup* form_group = MutableFormGroupForType(AutofillType(type));
  if (form_group) {
    is_client_validity_states_updated_ &=
        !IsClientValidationSupportedForType(type);
    form_group->SetRawInfo(type, value);
  }
}

void AutofillProfile::GetSupportedTypes(
    ServerFieldTypeSet* supported_types) const {
  FormGroupList info = FormGroups();
  for (const auto* form_group : info) {
    form_group->GetSupportedTypes(supported_types);
  }
}

bool AutofillProfile::IsEmpty(const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

bool AutofillProfile::IsPresentButInvalid(ServerFieldType type) const {
  std::string country = UTF16ToUTF8(GetRawInfo(ADDRESS_HOME_COUNTRY));
  base::string16 data = GetRawInfo(type);
  if (data.empty())
    return false;

  switch (type) {
    case ADDRESS_HOME_STATE:
      return country == "US" && !IsValidState(data);

    case ADDRESS_HOME_ZIP:
      return country == "US" && !IsValidZip(data);

    case PHONE_HOME_WHOLE_NUMBER:
      return !i18n::PhoneObject(data, country).IsValidNumber();

    case EMAIL_ADDRESS:
      return !IsValidEmailAddress(data);

    default:
      NOTREACHED();
      return false;
  }
}

int AutofillProfile::Compare(const AutofillProfile& profile) const {
  const ServerFieldType types[] = {
      NAME_FULL,
      NAME_FIRST,
      NAME_MIDDLE,
      NAME_LAST,
      COMPANY_NAME,
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
  };

  for (ServerFieldType type : types) {
    int comparison = GetRawInfo(type).compare(profile.GetRawInfo(type));
    if (comparison != 0) {
      return comparison;
    }
  }

  return 0;
}

bool AutofillProfile::EqualsSansOrigin(const AutofillProfile& profile) const {
  return guid() == profile.guid() &&
         language_code() == profile.language_code() &&
         is_client_validity_states_updated() ==
             profile.is_client_validity_states_updated() &&
         Compare(profile) == 0;
}

bool AutofillProfile::EqualsForSyncPurposes(const AutofillProfile& profile)
    const {
  return use_count() == profile.use_count() &&
         use_date() == profile.use_date() &&
         EqualsSansGuid(profile);
}

bool AutofillProfile::EqualsIncludingUsageStatsForTesting(
    const AutofillProfile& profile) const {
  return use_count() == profile.use_count() &&
         use_date() == profile.use_date() && *this == profile;
}

bool AutofillProfile::operator==(const AutofillProfile& profile) const {
  return guid() == profile.guid() && EqualsSansGuid(profile);
}

bool AutofillProfile::operator!=(const AutofillProfile& profile) const {
  return !operator==(profile);
}

bool AutofillProfile::IsSubsetOf(const AutofillProfile& profile,
                                 const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  return IsSubsetOfForFieldSet(profile, app_locale, types);
}

bool AutofillProfile::IsSubsetOfForFieldSet(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const ServerFieldTypeSet& types) const {
  AutofillProfileComparator comparator(app_locale);

  for (ServerFieldType type : types) {
    base::string16 value = GetRawInfo(type);
    if (value.empty())
      continue;

    if (type == NAME_FULL || type == ADDRESS_HOME_STREET_ADDRESS) {
      // Ignore the compound "full name" field type.  We are only interested in
      // comparing the constituent parts.  For example, if |this| has a middle
      // name saved, but |profile| lacks one, |profile| could still be a subset
      // of |this|.  Likewise, ignore the compound "street address" type, as we
      // are only interested in matching line-by-line.
      continue;
    } else if (AutofillType(type).group() == PHONE_HOME) {
      // Phone numbers should be canonicalized prior to being compared.
      if (type != PHONE_HOME_WHOLE_NUMBER) {
        continue;
      } else if (!i18n::PhoneNumbersMatch(
                     value, profile.GetRawInfo(type),
                     base::UTF16ToASCII(GetRawInfo(ADDRESS_HOME_COUNTRY)),
                     app_locale)) {
        return false;
      }
    } else {
      const base::string16 this_value =
          comparator.NormalizeForComparison(value);
      const base::string16 that_value =
          comparator.NormalizeForComparison(profile.GetRawInfo(type));
      if (this_value != that_value)
        return false;
    }
  }

  return true;
}

void AutofillProfile::OverwriteDataFrom(const AutofillProfile& profile) {
  // Verified profiles should never be overwritten with unverified data.
  DCHECK(!IsVerified() || profile.IsVerified());
  DCHECK_EQ(guid(), profile.guid());

  // Some fields should not got overwritten by empty values; back-up the
  // values.
  std::string language_code_value = language_code();
  std::string origin_value = origin();
  base::string16 name_full_value = GetRawInfo(NAME_FULL);

  *this = profile;

  if (origin().empty())
    set_origin(origin_value);
  if (language_code().empty())
    set_language_code(language_code_value);
  if (!HasRawInfo(NAME_FULL))
    SetRawInfo(NAME_FULL, name_full_value);
}

bool AutofillProfile::MergeDataFrom(const AutofillProfile& profile,
                                    const std::string& app_locale) {
  // Verified profiles should never be overwritten with unverified data.
  DCHECK(!IsVerified() || profile.IsVerified());
  AutofillProfileComparator comparator(app_locale);
  DCHECK(comparator.AreMergeable(*this, profile));

  NameInfo name;
  EmailInfo email;
  CompanyInfo company;
  PhoneNumber phone_number(this);
  Address address;

  DVLOG(1) << "Merging profiles:\nSource = " << profile << "\nDest = " << *this;

  // The comparator's merge operations are biased to prefer the data in the
  // first profile parameter when the data is the same modulo case. We expect
  // the caller to pass the incoming profile in this position to prefer
  // accepting updates instead of preserving the original data. I.e., passing
  // the incoming profile first accepts case and diacritic changes, for example,
  // the other ways does not.
  if (!comparator.MergeNames(profile, *this, &name) ||
      !comparator.MergeEmailAddresses(profile, *this, &email) ||
      !comparator.MergeCompanyNames(profile, *this, &company) ||
      !comparator.MergePhoneNumbers(profile, *this, &phone_number) ||
      !comparator.MergeAddresses(profile, *this, &address)) {
    NOTREACHED();
    return false;
  }

  // TODO(rogerm): As implemented, "origin" really denotes "domain of last use".
  // Find a better merge heuristic. Ditto for language code.
  set_origin(profile.origin());
  set_language_code(profile.language_code());

  // Update the use-count to be the max of the two merge-counts. Alternatively,
  // we could have summed the two merge-counts. We don't sum because it skews
  // the frecency value on merge and double counts usage on profile reuse.
  // Profile reuse is accounted for on RecordUseOf() on selection of a profile
  // in the autofill drop-down; we don't need to account for that here. Further,
  // a similar, fully-typed submission that merges to an existing profile should
  // not be counted as a re-use of that profile.
  set_use_count(std::max(profile.use_count(), use_count()));
  set_use_date(std::max(profile.use_date(), use_date()));

  // Update the fields which need to be modified, if any. Note: that we're
  // comparing the fields for representational equality below (i.e., are the
  // values byte for byte the same).

  bool modified = false;

  if (name_ != name) {
    name_ = name;
    modified = true;
  }

  if (email_ != email) {
    email_ = email;
    modified = true;
  }

  if (company_ != company) {
    company_ = company;
    modified = true;
  }

  if (phone_number_ != phone_number) {
    phone_number_ = phone_number;
    modified = true;
  }

  if (address_ != address) {
    address_ = address;
    modified = true;
  }

  is_client_validity_states_updated_ &= !modified;

  return modified;
}

bool AutofillProfile::SaveAdditionalInfo(const AutofillProfile& profile,
                                         const std::string& app_locale) {
  // If both profiles are verified, do not merge them.
  if (IsVerified() && profile.IsVerified())
    return false;

  AutofillProfileComparator comparator(app_locale);

  // SaveAdditionalInfo should not have been called if the profiles were not
  // already deemed to be mergeable.
  DCHECK(comparator.AreMergeable(*this, profile));

  // We don't replace verified profile data with unverified profile data. But,
  // we can merge two verified profiles or merge verified profile data into an
  // unverified profile.
  if (!IsVerified() || profile.IsVerified()) {
    if (MergeDataFrom(profile, app_locale)) {
      AutofillMetrics::LogProfileActionOnFormSubmitted(
          AutofillMetrics::EXISTING_PROFILE_UPDATED);
    } else {
      AutofillMetrics::LogProfileActionOnFormSubmitted(
          AutofillMetrics::EXISTING_PROFILE_USED);
    }
  }
  return true;
}

// static
bool AutofillProfile::SupportsMultiValue(ServerFieldType type) {
  FieldTypeGroup group = AutofillType(type).group();
  return group == NAME ||
         group == NAME_BILLING ||
         group == EMAIL ||
         group == PHONE_HOME ||
         group == PHONE_BILLING;
}

// static
void AutofillProfile::CreateDifferentiatingLabels(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    std::vector<base::string16>* labels) {
  const size_t kMinimalFieldsShown = 2;
  CreateInferredLabels(profiles, nullptr, UNKNOWN_TYPE, kMinimalFieldsShown,
                       app_locale, labels);
  DCHECK_EQ(profiles.size(), labels->size());
}

// static
void AutofillProfile::CreateInferredLabels(
    const std::vector<AutofillProfile*>& profiles,
    const std::vector<ServerFieldType>* suggested_fields,
    ServerFieldType excluded_field,
    size_t minimal_fields_shown,
    const std::string& app_locale,
    std::vector<base::string16>* labels) {
  std::vector<ServerFieldType> fields_to_use;
  GetFieldsForDistinguishingProfiles(suggested_fields, excluded_field,
                                     &fields_to_use);

  // Construct the default label for each profile. Also construct a map that
  // associates each label with the profiles that have this label. This map is
  // then used to detect which labels need further differentiating fields.
  std::map<base::string16, std::list<size_t> > labels_to_profiles;
  for (size_t i = 0; i < profiles.size(); ++i) {
    base::string16 label = profiles[i]->ConstructInferredLabel(
        fields_to_use.data(), fields_to_use.size(), minimal_fields_shown,
        app_locale);
    labels_to_profiles[label].push_back(i);
  }

  labels->resize(profiles.size());
  for (auto& it : labels_to_profiles) {
    if (it.second.size() == 1) {
      // This label is unique, so use it without any further ado.
      base::string16 label = it.first;
      size_t profile_index = it.second.front();
      (*labels)[profile_index] = label;
    } else {
      // We have more than one profile with the same label, so add
      // differentiating fields.
      CreateInferredLabelsHelper(profiles, it.second, fields_to_use,
                                 minimal_fields_shown, app_locale, labels);
    }
  }
}

base::string16 AutofillProfile::ConstructInferredLabel(
    const ServerFieldType* included_fields,
    const size_t included_fields_size,
    size_t num_fields_to_use,
    const std::string& app_locale) const {
  // TODO(estade): use libaddressinput?
  base::string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);

  AutofillType region_code_type(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const base::string16& profile_region_code =
      GetInfo(region_code_type, app_locale);
  std::string address_region_code = UTF16ToUTF8(profile_region_code);

  // A copy of |this| pruned down to contain only data for the address fields in
  // |included_fields|.
  AutofillProfile trimmed_profile(guid(), origin());
  trimmed_profile.SetInfo(region_code_type, profile_region_code, app_locale);
  trimmed_profile.set_language_code(language_code());

  std::vector<ServerFieldType> remaining_fields;
  for (size_t i = 0; i < included_fields_size && num_fields_to_use > 0; ++i) {
    ::i18n::addressinput::AddressField address_field;
    if (!i18n::FieldForType(included_fields[i], &address_field) ||
        !::i18n::addressinput::IsFieldUsed(address_field,
                                           address_region_code) ||
        address_field == ::i18n::addressinput::COUNTRY) {
      remaining_fields.push_back(included_fields[i]);
      continue;
    }

    AutofillType autofill_type(included_fields[i]);
    base::string16 field_value = GetInfo(autofill_type, app_locale);
    if (field_value.empty())
      continue;

    trimmed_profile.SetInfo(autofill_type, field_value, app_locale);
    --num_fields_to_use;
  }

  std::unique_ptr<AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(trimmed_profile, app_locale);
  std::string address_line;
  ::i18n::addressinput::GetFormattedNationalAddressLine(
      *address_data, &address_line);
  base::string16 label = base::UTF8ToUTF16(address_line);

  for (std::vector<ServerFieldType>::const_iterator it =
           remaining_fields.begin();
       it != remaining_fields.end() && num_fields_to_use > 0;
       ++it) {
    base::string16 field_value;
    // Special case whole numbers: we want the user-formatted (raw) version, not
    // the canonicalized version we'll fill into the page.
    if (*it == PHONE_HOME_WHOLE_NUMBER)
      field_value = GetRawInfo(*it);
    else
      field_value = GetInfo(AutofillType(*it), app_locale);
    if (field_value.empty())
      continue;

    if (!label.empty())
      label.append(separator);

    label.append(field_value);
    --num_fields_to_use;
  }

  // If country code is missing, libaddressinput won't be used to format the
  // address. In this case the suggestion might include a multi-line street
  // address which needs to be flattened.
  base::ReplaceChars(label, base::ASCIIToUTF16("\n"), separator, &label);

  return label;
}

void AutofillProfile::GenerateServerProfileIdentifier() {
  DCHECK_EQ(SERVER_PROFILE, record_type());
  base::string16 contents = GetRawInfo(NAME_FIRST);
  contents.append(GetRawInfo(NAME_MIDDLE));
  contents.append(GetRawInfo(NAME_LAST));
  contents.append(GetRawInfo(EMAIL_ADDRESS));
  contents.append(GetRawInfo(COMPANY_NAME));
  contents.append(GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
  contents.append(GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
  contents.append(GetRawInfo(ADDRESS_HOME_CITY));
  contents.append(GetRawInfo(ADDRESS_HOME_STATE));
  contents.append(GetRawInfo(ADDRESS_HOME_ZIP));
  contents.append(GetRawInfo(ADDRESS_HOME_SORTING_CODE));
  contents.append(GetRawInfo(ADDRESS_HOME_COUNTRY));
  contents.append(GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  std::string contents_utf8 = UTF16ToUTF8(contents);
  contents_utf8.append(language_code());
  server_id_ = base::SHA1HashString(contents_utf8);
}

void AutofillProfile::RecordAndLogUse() {
  previous_use_date_ = use_date();
  UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.Profile",
                            (AutofillClock::Now() - use_date()).InDays());
  RecordUse();
}

bool AutofillProfile::IsAnInvalidPhoneNumber(ServerFieldType type) const {
  if (GetValidityState(type, SERVER) == VALID ||
      (type != PHONE_HOME_WHOLE_NUMBER && type != PHONE_HOME_NUMBER &&
       type != PHONE_BILLING_WHOLE_NUMBER && type != PHONE_BILLING_NUMBER))
    return false;
  if (GetValidityState(type, SERVER) == INVALID)
    return true;

  ServerFieldTypeSet types;
  if (GroupTypeOfServerFieldType(type) == PHONE_HOME) {
    types = {PHONE_HOME_NUMBER, PHONE_HOME_CITY_CODE,
             PHONE_HOME_CITY_AND_NUMBER};
    if (type == PHONE_HOME_WHOLE_NUMBER) {
      types.insert(PHONE_HOME_WHOLE_NUMBER);
      types.insert(PHONE_HOME_COUNTRY_CODE);
    }
  } else if (GroupTypeOfServerFieldType(type) == PHONE_BILLING) {
    types = {PHONE_BILLING_NUMBER, PHONE_BILLING_CITY_CODE,
             PHONE_BILLING_CITY_AND_NUMBER};
    if (type == PHONE_BILLING_WHOLE_NUMBER) {
      types.insert(PHONE_BILLING_WHOLE_NUMBER);
      types.insert(PHONE_BILLING_COUNTRY_CODE);
    }
  }

  for (const auto& cur_type : types) {
    if (GetValidityState(cur_type, SERVER) == INVALID)
      return true;
  }
  return false;
}

AutofillProfile::ValidityState AutofillProfile::GetValidityState(
    ServerFieldType type,
    ValidationSource validation_source) const {
  if (validation_source == CLIENT) {
    type = NormalizeTypeForValidityCheck(type);
    // Return UNSUPPORTED for types that autofill does not validate.
    if (!IsClientValidationSupportedForType(type))
      return UNSUPPORTED;

    auto it = client_validity_states_.find(type);
    return (it == client_validity_states_.end()) ? UNVALIDATED : it->second;
  }
  DCHECK_EQ(SERVER, validation_source);

  auto it = server_validity_states_.find(type);
  return (it == server_validity_states_.end()) ? UNVALIDATED : it->second;
}

void AutofillProfile::SetValidityState(ServerFieldType type,
                                       ValidityState validity,
                                       ValidationSource validation_source) {
  if (validation_source == CLIENT) {
    // Do not save validity of unsupported types.
    if (!IsClientValidationSupportedForType(type))
      return;
    client_validity_states_[type] = validity;
    return;
  }
  DCHECK_EQ(SERVER, validation_source);
  server_validity_states_[type] = validity;
}

void AutofillProfile::UpdateServerValidityMap(
    const ProfileValidityMap& validity_map) {
  server_validity_states_.clear();

  const auto& field_validity_states = validity_map.field_validity_states();
  for (auto current_pair = field_validity_states.begin();
       current_pair != field_validity_states.end(); ++current_pair) {
    const auto& field_type = static_cast<ServerFieldType>(current_pair->first);
    const auto& field_validity =
        static_cast<ValidityState>(current_pair->second);
    server_validity_states_[field_type] = field_validity;
  }
}

// static
bool AutofillProfile::IsClientValidationSupportedForType(ServerFieldType type) {
  for (auto supported_type : kSupportedTypesByClientForValidation) {
    if (type == supported_type)
      return true;
  }
  return false;
}

int AutofillProfile::GetClientValidityBitfieldValue() const {
  int validity_value = 0;
  size_t field_type_shift = 0;
  for (ServerFieldType supported_type : kSupportedTypesByClientForValidation) {
    DCHECK(GetValidityState(supported_type, CLIENT) != UNSUPPORTED);
    validity_value |= GetValidityState(supported_type, CLIENT)
                      << field_type_shift;
    field_type_shift += kValidityBitsPerType;
  }

  // Check the the shift is still in range.
  DCHECK_LE(field_type_shift, 64U);

  return validity_value;
}

void AutofillProfile::SetClientValidityFromBitfieldValue(int bitfield_value) {
  // Compute the bitmask based on the number a bits per type. For example, this
  // could be the two least significant bits (0b11).
  const int kBitmask = (1 << kValidityBitsPerType) - 1;

  for (ServerFieldType supported_type : kSupportedTypesByClientForValidation) {
    // Apply the bitmask to the bitfield value to get the validity value of the
    // current |supported_type|.
    int validity_value = bitfield_value & kBitmask;
    if (validity_value < 0 || validity_value >= UNSUPPORTED) {
      NOTREACHED();
      continue;
    }

    SetValidityState(supported_type, static_cast<ValidityState>(validity_value),
                     CLIENT);

    // Shift the bitfield value to access the validity of the next field type.
    bitfield_value = bitfield_value >> kValidityBitsPerType;
  }
}

base::string16 AutofillProfile::GetInfoImpl(
    const AutofillType& type,
    const std::string& app_locale) const {
  if (type.html_type() == HTML_TYPE_FULL_ADDRESS) {
    std::unique_ptr<AddressData> address_data =
        i18n::CreateAddressDataFromAutofillProfile(*this, app_locale);
    if (!addressinput::HasAllRequiredFields(*address_data))
      return base::string16();

    std::vector<std::string> lines;
    ::i18n::addressinput::GetFormattedNationalAddress(*address_data, &lines);
    return base::UTF8ToUTF16(base::JoinString(lines, "\n"));
  }

  const FormGroup* form_group = FormGroupForType(type);
  if (!form_group)
    return base::string16();

  return form_group->GetInfoImpl(type, app_locale);
}

bool AutofillProfile::SetInfoImpl(const AutofillType& type,
                                  const base::string16& value,
                                  const std::string& app_locale) {
  FormGroup* form_group = MutableFormGroupForType(type);
  if (!form_group)
    return false;

  is_client_validity_states_updated_ &=
      !IsClientValidationSupportedForType(type.GetStorableType());

  base::string16 trimmed_value;
  base::TrimWhitespace(value, base::TRIM_ALL, &trimmed_value);
  return form_group->SetInfoImpl(type, trimmed_value, app_locale);
}

// static
void AutofillProfile::CreateInferredLabelsHelper(
    const std::vector<AutofillProfile*>& profiles,
    const std::list<size_t>& indices,
    const std::vector<ServerFieldType>& fields,
    size_t num_fields_to_include,
    const std::string& app_locale,
    std::vector<base::string16>* labels) {
  // For efficiency, we first construct a map of fields to their text values and
  // each value's frequency.
  std::map<ServerFieldType,
           std::map<base::string16, size_t> > field_text_frequencies_by_field;
  for (const ServerFieldType& field : fields) {
    std::map<base::string16, size_t>& field_text_frequencies =
        field_text_frequencies_by_field[field];

    for (const auto& it : indices) {
      const AutofillProfile* profile = profiles[it];
      base::string16 field_text =
          profile->GetInfo(AutofillType(field), app_locale);

      // If this label is not already in the map, add it with frequency 0.
      if (!field_text_frequencies.count(field_text))
        field_text_frequencies[field_text] = 0;

      // Now, increment the frequency for this label.
      ++field_text_frequencies[field_text];
    }
  }

  // Now comes the meat of the algorithm. For each profile, we scan the list of
  // fields to use, looking for two things:
  //  1. A (non-empty) field that differentiates the profile from all others
  //  2. At least |num_fields_to_include| non-empty fields
  // Before we've satisfied condition (2), we include all fields, even ones that
  // are identical across all the profiles. Once we've satisfied condition (2),
  // we only include fields that that have at last two distinct values.
  for (const auto& it : indices) {
    const AutofillProfile* profile = profiles[it];

    std::vector<ServerFieldType> label_fields;
    bool found_differentiating_field = false;
    for (auto field = fields.begin(); field != fields.end(); ++field) {
      // Skip over empty fields.
      base::string16 field_text =
          profile->GetInfo(AutofillType(*field), app_locale);
      if (field_text.empty())
        continue;

      std::map<base::string16, size_t>& field_text_frequencies =
          field_text_frequencies_by_field[*field];
      found_differentiating_field |=
          !field_text_frequencies.count(base::string16()) &&
          (field_text_frequencies[field_text] == 1);

      // Once we've found enough non-empty fields, skip over any remaining
      // fields that are identical across all the profiles.
      if (label_fields.size() >= num_fields_to_include &&
          (field_text_frequencies.size() == 1))
        continue;

      label_fields.push_back(*field);

      // If we've (1) found a differentiating field and (2) found at least
      // |num_fields_to_include| non-empty fields, we're done!
      if (found_differentiating_field &&
          label_fields.size() >= num_fields_to_include)
        break;
    }

    (*labels)[it] = profile->ConstructInferredLabel(
        label_fields.data(), label_fields.size(), label_fields.size(),
        app_locale);
  }
}

AutofillProfile::FormGroupList AutofillProfile::FormGroups() const {
  FormGroupList v(5);
  v[0] = &name_;
  v[1] = &email_;
  v[2] = &company_;
  v[3] = &phone_number_;
  v[4] = &address_;
  return v;
}

const FormGroup* AutofillProfile::FormGroupForType(
    const AutofillType& type) const {
  return const_cast<AutofillProfile*>(this)->MutableFormGroupForType(type);
}

FormGroup* AutofillProfile::MutableFormGroupForType(const AutofillType& type) {
  switch (type.group()) {
    case NAME:
    case NAME_BILLING:
      return &name_;

    case EMAIL:
      return &email_;

    case COMPANY:
      return &company_;

    case PHONE_HOME:
    case PHONE_BILLING:
      return &phone_number_;

    case ADDRESS_HOME:
    case ADDRESS_BILLING:
      return &address_;

    case NO_GROUP:
    case CREDIT_CARD:
    case PASSWORD_FIELD:
    case USERNAME_FIELD:
    case TRANSACTION:
    case UNFILLABLE:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

bool AutofillProfile::EqualsSansGuid(const AutofillProfile& profile) const {
  return origin() == profile.origin() &&
         language_code() == profile.language_code() &&
         is_client_validity_states_updated() ==
             profile.is_client_validity_states_updated() &&
         Compare(profile) == 0;
}

std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile) {
  return os << profile.guid() << " " << profile.origin() << " "
            << UTF16ToUTF8(profile.GetRawInfo(NAME_FULL)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(NAME_FIRST)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(NAME_MIDDLE)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(NAME_LAST)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(EMAIL_ADDRESS)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(COMPANY_NAME)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE1)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE2)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE3)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY))
            << " " << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_CITY)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STATE)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_ZIP)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE)) << " "
            << UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)) << " "
            << profile.language_code() << " "
            << UTF16ToUTF8(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER)) << " "
            << profile.GetClientValidityBitfieldValue() << " "
            << profile.use_count() << " " << profile.use_date();
}

}  // namespace autofill
