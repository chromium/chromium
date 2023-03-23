// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_profile.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <set>

#include "base/guid.h"
#include "base/hash/sha1.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/contact_info.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
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
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;

namespace autofill {

namespace {

// Stores the data types that are relevant for the structured address/name.
const std::array<ServerFieldType, 7> kStructuredDataTypes = {
    NAME_FIRST,
    NAME_MIDDLE,
    NAME_LAST,
    NAME_LAST_FIRST,
    NAME_LAST_SECOND,
    ADDRESS_HOME_STREET_NAME,
    ADDRESS_HOME_HOUSE_NUMBER};

// Like |AutofillType::GetStorableType()|, but also returns |NAME_FULL| for
// first, middle, and last name field types, and groups phone number types
// similarly.
ServerFieldType GetStorableTypeCollapsingGroups(ServerFieldType type) {
  ServerFieldType storable_type = AutofillType(type).GetStorableType();
  if (AutofillType(storable_type).group() == FieldTypeGroup::kName)
    return NAME_FULL;

  if (AutofillType(storable_type).group() == FieldTypeGroup::kPhoneHome)
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
        kDefaultDistinguishingFields + std::size(kDefaultDistinguishingFields));
    if (excluded_field == UNKNOWN_TYPE) {
      distinguishing_fields->swap(default_fields);
      return;
    }
    suggested_fields = &default_fields;
  }

  // Keep track of which fields we've seen so that we avoid duplicate entries.
  // Always ignore fields of unknown type and the excluded field.
  ServerFieldTypeSet seen_fields;
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

}  // namespace

AutofillProfile::AutofillProfile(const std::string& guid,
                                 const std::string& origin,
                                 Source source)
    : AutofillDataModel(guid, origin),
      company_(this),
      phone_number_(this),
      record_type_(LOCAL_PROFILE),
      has_converted_(false),
      source_(source) {}

// TODO(crbug.com/1177366): Remove this constructor.
AutofillProfile::AutofillProfile(RecordType type, const std::string& server_id)
    : AutofillDataModel(base::GenerateGUID(), std::string()),
      company_(this),
      phone_number_(this),
      server_id_(server_id),
      record_type_(type),
      has_converted_(false),
      source_(Source::kLocalOrSyncable) {
  DCHECK(type == SERVER_PROFILE);
}

AutofillProfile::AutofillProfile(Source source)
    : AutofillProfile(base::GenerateGUID(), std::string(), source) {}

AutofillProfile::AutofillProfile()
    : AutofillProfile(Source::kLocalOrSyncable) {}

AutofillProfile::AutofillProfile(const AutofillProfile& profile)
    : AutofillDataModel(std::string(), std::string()),
      company_(this),
      phone_number_(this) {
  operator=(profile);
}

AutofillProfile::~AutofillProfile() = default;

AutofillProfile& AutofillProfile::operator=(const AutofillProfile& profile) {
  if (this == &profile)
    return *this;

  set_disallow_settings_visible_updates(
      profile.disallow_settings_visible_updates());
  set_use_count(profile.use_count());
  set_use_date(profile.use_date());
  set_previous_use_date(profile.previous_use_date());
  set_modification_date(profile.modification_date());

  set_guid(profile.guid());
  set_origin(profile.origin());

  set_profile_label(profile.profile_label());

  record_type_ = profile.record_type_;

  name_ = profile.name_;
  email_ = profile.email_;
  company_ = profile.company_;
  company_.set_profile(this);
  phone_number_ = profile.phone_number_;
  phone_number_.set_profile(this);
  birthdate_ = profile.birthdate_;

  address_ = profile.address_;
  set_language_code(profile.language_code());

  server_id_ = profile.server_id();
  has_converted_ = profile.has_converted();

  source_ = profile.source_;
  initial_creator_id_ = profile.initial_creator_id_;
  last_modifier_id_ = profile.last_modifier_id_;

  return *this;
}

AutofillMetadata AutofillProfile::GetMetadata() const {
  AutofillMetadata metadata = AutofillDataModel::GetMetadata();
  metadata.id = (record_type_ == LOCAL_PROFILE ? guid() : server_id_);
  metadata.has_converted = has_converted_;
  return metadata;
}

double AutofillProfile::GetRankingScore(base::Time current_time) const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableRankingFormulaAddressProfiles)) {
    // Exponentially decay the use count by the days since the data model was
    // last used.
    return log10(use_count() + 1) *
           exp(-GetDaysSinceLastUse(current_time) /
               features::kAutofillRankingFormulaAddressProfilesUsageHalfLife
                   .Get());
  }
  // Default to legacy frecency scoring.
  return AutofillDataModel::GetRankingScore(current_time);
}

bool AutofillProfile::SetMetadata(const AutofillMetadata& metadata) {
  // Make sure the ids matches.
  if (metadata.id != (record_type_ == LOCAL_PROFILE ? guid() : server_id_))
    return false;

  if (!AutofillDataModel::SetMetadata(metadata))
    return false;

  has_converted_ = metadata.has_converted;
  return true;
}

bool AutofillProfile::IsDeletable() const {
  return AutofillDataModel::IsDeletable() && !IsVerified();
}

void AutofillProfile::GetMatchingTypes(
    const std::u16string& text,
    const std::string& app_locale,
    ServerFieldTypeSet* matching_types) const {
  ServerFieldTypeSet matching_types_in_this_profile;
  for (const auto* form_group : FormGroups()) {
    form_group->GetMatchingTypes(text, app_locale,
                                 &matching_types_in_this_profile);
  }

  for (auto type : matching_types_in_this_profile) {
    matching_types->insert(type);
  }
}

std::u16string AutofillProfile::GetRawInfo(ServerFieldType type) const {
  const FormGroup* form_group = FormGroupForType(AutofillType(type));
  if (!form_group)
    return std::u16string();
  return form_group->GetRawInfo(type);
}

int AutofillProfile::GetRawInfoAsInt(ServerFieldType type) const {
  const FormGroup* form_group = FormGroupForType(AutofillType(type));
  if (!form_group)
    return 0;
  return form_group->GetRawInfoAsInt(type);
}

void AutofillProfile::SetRawInfoWithVerificationStatus(
    ServerFieldType type,
    const std::u16string& value,
    VerificationStatus status) {
  FormGroup* form_group = MutableFormGroupForType(AutofillType(type));
  if (form_group) {
    form_group->SetRawInfoWithVerificationStatus(type, value, status);
  }
}

void AutofillProfile::SetRawInfoAsIntWithVerificationStatus(
    ServerFieldType type,
    int value,
    VerificationStatus status) {
  FormGroup* form_group = MutableFormGroupForType(AutofillType(type));
  if (form_group) {
    form_group->SetRawInfoAsIntWithVerificationStatus(type, value, status);
  }
}

void AutofillProfile::GetSupportedTypes(
    ServerFieldTypeSet* supported_types) const {
  for (const auto* form_group : FormGroups()) {
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
  std::u16string data = GetRawInfo(type);
  if (data.empty())
    return false;

  switch (type) {
    case ADDRESS_HOME_STATE:
      return country == "US" && !IsValidState(data);

    case ADDRESS_HOME_ZIP:
      return country == "US" && !IsValidZip(data);

    case PHONE_HOME_WHOLE_NUMBER:
      return !i18n::PhoneObject(data, country, /*infer_country_code=*/false)
                  .IsValidNumber();

    case EMAIL_ADDRESS:
      return !IsValidEmailAddress(data);

    default:
      NOTREACHED();
      return false;
  }
}

int AutofillProfile::Compare(const AutofillProfile& profile) const {
  const ServerFieldType types[] = {
      // TODO(crbug.com/1113617): Honorifics are temporally disabled.
      // NAME_HONORIFIC_PREFIX,
      NAME_FULL,
      NAME_FIRST,
      NAME_MIDDLE,
      NAME_LAST,
      NAME_LAST_FIRST,
      NAME_LAST_SECOND,
      NAME_LAST_CONJUNCTION,
      COMPANY_NAME,
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
      ADDRESS_HOME_HOUSE_NUMBER,
      ADDRESS_HOME_STREET_NAME,
      ADDRESS_HOME_DEPENDENT_STREET_NAME,
      ADDRESS_HOME_PREMISE_NAME,
      ADDRESS_HOME_SUBPREMISE,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
  };

  for (ServerFieldType type : types) {
    int comparison = GetRawInfo(type).compare(profile.GetRawInfo(type));
    if (comparison != 0) {
      return comparison;
    }

    // If the value is empty, the verification status can be ambiguous because
    // the value could be either build from its empty child nodes or parsed
    // from its parent. Therefore, it should not be considered when evaluating
    // the similarity of two profiles.
    if (profile.GetRawInfo(type).empty())
      continue;

    if (IsLessSignificantVerificationStatus(
            GetVerificationStatus(type), profile.GetVerificationStatus(type))) {
      return -1;
    }
    if (IsLessSignificantVerificationStatus(profile.GetVerificationStatus(type),
                                            GetVerificationStatus(type))) {
      return 1;
    }
  }

  return 0;
}

bool AutofillProfile::EqualsForSyncPurposes(
    const AutofillProfile& profile) const {
  return use_count() == profile.use_count() &&
         UseDateEqualsInSeconds(&profile) && EqualsSansGuid(profile);
}

bool AutofillProfile::EqualsForUpdatePurposes(
    const AutofillProfile& new_profile) const {
  return use_count() == new_profile.use_count() &&
         (origin() == new_profile.origin() || !new_profile.IsVerified()) &&
         UseDateEqualsInSeconds(&new_profile) &&
         language_code() == new_profile.language_code() &&
         Compare(new_profile) == 0;
}

bool AutofillProfile::EqualsIncludingUsageStatsForTesting(
    const AutofillProfile& profile) const {
  return use_count() == profile.use_count() &&
         UseDateEqualsInSeconds(&profile) && *this == profile;
}

bool AutofillProfile::operator==(const AutofillProfile& profile) const {
  return guid() == profile.guid() && EqualsSansGuid(profile);
}

bool AutofillProfile::operator!=(const AutofillProfile& profile) const {
  return !operator==(profile);
}

bool AutofillProfile::IsSubsetOfForFieldSet(
    const AutofillProfileComparator& comparator,
    const AutofillProfile& profile,
    const std::string& app_locale,
    const ServerFieldTypeSet& types) const {
  for (ServerFieldType type : types) {
    // Prefer GetInfo over GetRawInfo so that a reasonable value is retrieved
    // when the raw data is empty or unnormalized. For example, suppose a
    // profile's first and last names are set but its full name is not set.
    // GetInfo for the NAME_FULL type returns the constituent name parts;
    // however, GetRawInfo returns an empty string.
    const std::u16string value = GetInfo(type, app_locale);

    if (value.empty() || type == ADDRESS_HOME_STREET_ADDRESS ||
        type == ADDRESS_HOME_LINE1 || type == ADDRESS_HOME_LINE2 ||
        type == ADDRESS_HOME_LINE3) {
      // Ignore street addresses because comparing addresses such as 200 Elm St
      // and 200 Elm Street could cause |profile| to not be seen as a subset of
      // |this|. If the form includes a street address, then it is likely it
      // contains another address field, e.g. a city or postal code, and
      // comparing these other address parts is more reliable.
      continue;
    } else if (type == NAME_FULL) {
      if (!comparator.IsNameVariantOf(
              comparator.NormalizeForComparison(
                  profile.GetInfo(NAME_FULL, app_locale)),
              comparator.NormalizeForComparison(value))) {
        // Check whether the full name of |this| can be derived from the full
        // name of |profile| if the form contains a full name field.
        //
        // Suppose the full name of |this| is Mia Park and |profile|'s full name
        // is Mia L Park. Mia Park can be derived from Mia L Park, so |this|
        // could be a subset of |profile|.
        //
        // If the form contains fields for a name's constiuent parts, e.g.
        // NAME_FIRST, then these values are compared according to the
        // conditions that follow.
        return false;
      }
    } else if (AutofillType(type).group() == FieldTypeGroup::kPhoneHome) {
      // Phone numbers should be canonicalized before comparing.
      if (type != PHONE_HOME_WHOLE_NUMBER &&
          type != PHONE_HOME_CITY_AND_NUMBER) {
        continue;
      } else if (!i18n::PhoneNumbersMatch(
                     value, profile.GetInfo(type, app_locale),
                     base::UTF16ToASCII(GetRawInfo(ADDRESS_HOME_COUNTRY)),
                     app_locale)) {
        return false;
      }
    } else if (!comparator.Compare(value, profile.GetInfo(type, app_locale))) {
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

  // Structured names should not be simply overwritten but it should be
  // attempted to merge the names.
  bool is_structured_name_mergeable = false;
  NameInfo name_info = GetNameInfo();
  is_structured_name_mergeable =
      name_info.IsStructuredNameMergeable(profile.GetNameInfo());
  name_info.MergeStructuredName(profile.GetNameInfo());

  *this = profile;

  if (origin().empty())
    set_origin(origin_value);
  if (language_code().empty())
    set_language_code(language_code_value);

  // For structured names, use the merged name if possible.
  if (is_structured_name_mergeable) {
    name_ = name_info;
    return;
  }
  // For structured names, if the full name of |profile| is empty, maintain the
  // complete name structure. Note, this should only happen if the complete name
  // is empty.  For the legacy implementation, set the full name if |profile|
  // does not contain a full name.
  if (!HasRawInfo(NAME_FULL)) {
    name_ = name_info;
  }
}

bool AutofillProfile::MergeStructuredDataFrom(const AutofillProfile& profile,
                                              const std::string& app_locale) {
  // Should only be called if the profile is already verified.
  DCHECK(IsVerified());

  AutofillProfileComparator comparator(app_locale);

  DVLOG(1) << "Merging profile structure information :\nSource = " << profile
           << "\nDest = " << *this;

  bool merged = false;

  // It is already verified upstream that the profiles and therefore also the
  // names and addresses are mergeable.
  // However, the structure should only be merged if the full names or addresses
  // are token equivalent.
  if (AreStringTokenEquivalent(GetRawInfo(NAME_FULL),
                               profile.GetRawInfo(NAME_FULL))) {
    NameInfo name;
    if (!comparator.MergeNames(profile, *this, name)) {
      NOTREACHED();
      return false;
    }
    if (name_ != name) {
      name_ = name;
      merged = true;
    }
  }

  if (AreStringTokenEquivalent(
          GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
          profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS))) {
    Address address;
    if (!comparator.MergeAddresses(profile, *this, address)) {
      NOTREACHED();
      return false;
    }
    if (address_ != address) {
      address_ = address;
      merged = true;
    }
  }

  return merged;
}

bool AutofillProfile::MergeDataFrom(const AutofillProfile& profile,
                                    const std::string& app_locale) {
  AutofillProfileComparator comparator(app_locale);
  DCHECK(comparator.AreMergeable(*this, profile));

  NameInfo name;
  EmailInfo email;
  CompanyInfo company(this);
  PhoneNumber phone_number(this);
  Address address;
  Birthdate birthdate;

  DVLOG(1) << "Merging profiles:\nSource = " << profile << "\nDest = " << *this;

  // The comparator's merge operations are biased to prefer the data in the
  // first profile parameter when the data is the same modulo case. We expect
  // the caller to pass the incoming profile in this position to prefer
  // accepting updates instead of preserving the original data. I.e., passing
  // the incoming profile first accepts case and diacritic changes, for example,
  // the other ways does not.
  if (!comparator.MergeNames(profile, *this, name) ||
      !comparator.MergeEmailAddresses(profile, *this, email) ||
      !comparator.MergeCompanyNames(profile, *this, company) ||
      !comparator.MergePhoneNumbers(profile, *this, phone_number) ||
      !comparator.MergeAddresses(profile, *this, address) ||
      !comparator.MergeBirthdates(profile, *this, birthdate)) {
    NOTREACHED();
    return false;
  }

  // TODO(rogerm): As implemented, "origin" really denotes "domain of last use".
  // Find a better merge heuristic. Ditto for language code.
  set_origin(profile.origin());
  set_language_code(profile.language_code());

  // Update the use-count to be the max of the two merge-counts. Alternatively,
  // we could have summed the two merge-counts. We don't sum because it skews
  // the ranking score value on merge and double counts usage on profile reuse.
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

  if (birthdate_ != birthdate) {
    birthdate_ = birthdate;
    modified = true;
  }

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
  // we can merge two unverified profiles or merge verified profile data into an
  // unverified profile.
  if (!IsVerified() || profile.IsVerified()) {
    if (MergeDataFrom(profile, app_locale)) {
      AutofillMetrics::LogProfileActionOnFormSubmitted(
          AutofillMetrics::EXISTING_PROFILE_UPDATED);
    } else {
      AutofillMetrics::LogProfileActionOnFormSubmitted(
          AutofillMetrics::EXISTING_PROFILE_USED);
    }
    return true;
  }
  // If the profile is verified, only the structured name information should be
  // merged.
  if (MergeStructuredDataFrom(profile, app_locale)) {
    AutofillMetrics::LogProfileActionOnFormSubmitted(
        AutofillMetrics::EXISTING_PROFILE_UPDATED);
  } else {
    AutofillMetrics::LogProfileActionOnFormSubmitted(
        AutofillMetrics::EXISTING_PROFILE_USED);
  }
  return true;
}

// static
void AutofillProfile::CreateDifferentiatingLabels(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    std::vector<std::u16string>* labels) {
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
    std::vector<std::u16string>* labels) {
  std::vector<ServerFieldType> fields_to_use;
  GetFieldsForDistinguishingProfiles(suggested_fields, excluded_field,
                                     &fields_to_use);

  // Construct the default label for each profile. Also construct a map that
  // associates each label with the profiles that have this label. This map is
  // then used to detect which labels need further differentiating fields.
  std::map<std::u16string, std::list<size_t>> labels_to_profiles;
  for (size_t i = 0; i < profiles.size(); ++i) {
    std::u16string label = profiles[i]->ConstructInferredLabel(
        fields_to_use.data(), fields_to_use.size(), minimal_fields_shown,
        app_locale);
    labels_to_profiles[label].push_back(i);
  }

  labels->resize(profiles.size());
  for (auto& it : labels_to_profiles) {
    if (it.second.size() == 1) {
      // This label is unique, so use it without any further ado.
      std::u16string label = it.first;
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

std::u16string AutofillProfile::ConstructInferredLabel(
    const ServerFieldType* included_fields,
    const size_t included_fields_size,
    size_t num_fields_to_use,
    const std::string& app_locale) const {
  // TODO(estade): use libaddressinput?
  std::u16string separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);

  AutofillType region_code_type(HtmlFieldType::kCountryCode,
                                HtmlFieldMode::kNone);
  const std::u16string& profile_region_code =
      GetInfo(region_code_type, app_locale);
  std::string address_region_code = UTF16ToUTF8(profile_region_code);

  // A copy of |this| pruned down to contain only data for the address fields in
  // |included_fields|.
  AutofillProfile trimmed_profile(guid(), origin());
  trimmed_profile.SetInfo(region_code_type, profile_region_code, app_locale);
  trimmed_profile.set_language_code(language_code());
  AutofillCountry country(address_region_code);

  std::vector<ServerFieldType> remaining_fields;
  for (size_t i = 0; i < included_fields_size && num_fields_to_use > 0; ++i) {
    ::i18n::addressinput::AddressField address_field;
    if (!i18n::FieldForType(included_fields[i], &address_field) ||
        !country.IsAddressFieldSettingAccessible(address_field) ||
        address_field == ::i18n::addressinput::COUNTRY) {
      remaining_fields.push_back(included_fields[i]);
      continue;
    }

    AutofillType autofill_type(included_fields[i]);
    std::u16string field_value = GetInfo(autofill_type, app_locale);
    if (field_value.empty())
      continue;

    trimmed_profile.SetInfo(autofill_type, field_value, app_locale);
    --num_fields_to_use;
  }

  std::unique_ptr<AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(trimmed_profile, app_locale);
  std::string address_line;
  ::i18n::addressinput::GetFormattedNationalAddressLine(*address_data,
                                                        &address_line);
  std::u16string label = base::UTF8ToUTF16(address_line);

  for (std::vector<ServerFieldType>::const_iterator it =
           remaining_fields.begin();
       it != remaining_fields.end() && num_fields_to_use > 0; ++it) {
    std::u16string field_value;
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
  base::ReplaceChars(label, u"\n", separator, &label);

  return label;
}

void AutofillProfile::GenerateServerProfileIdentifier() {
  DCHECK_EQ(SERVER_PROFILE, record_type());
  std::u16string contents = GetRawInfo(NAME_FIRST);
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
  set_previous_use_date(use_date());
  UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.Profile",
                            (AutofillClock::Now() - use_date()).InDays());
  RecordUse();
  LogVerificationStatuses();
}

void AutofillProfile::LogVerificationStatuses() {
  AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(*this);
  AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(*this);
}

VerificationStatus AutofillProfile::GetVerificationStatusImpl(
    const ServerFieldType type) const {
  const FormGroup* form_group = FormGroupForType(AutofillType(type));
  if (!form_group)
    return VerificationStatus::kNoStatus;

  return form_group->GetVerificationStatus(type);
}

std::u16string AutofillProfile::GetInfoImpl(
    const AutofillType& type,
    const std::string& app_locale) const {
  if (type.html_type() == HtmlFieldType::kFullAddress) {
    std::unique_ptr<AddressData> address_data =
        i18n::CreateAddressDataFromAutofillProfile(*this, app_locale);
    if (!addressinput::HasAllRequiredFields(*address_data))
      return std::u16string();

    std::vector<std::string> lines;
    ::i18n::addressinput::GetFormattedNationalAddress(*address_data, &lines);
    return base::UTF8ToUTF16(base::JoinString(lines, "\n"));
  }

  const FormGroup* form_group = FormGroupForType(type);
  if (!form_group)
    return std::u16string();

  return form_group->GetInfoImpl(type, app_locale);
}

bool AutofillProfile::SetInfoWithVerificationStatusImpl(
    const AutofillType& type,
    const std::u16string& value,
    const std::string& app_locale,
    VerificationStatus status) {
  FormGroup* form_group = MutableFormGroupForType(type);
  if (!form_group)
    return false;

  std::u16string trimmed_value;
  base::TrimWhitespace(value, base::TRIM_ALL, &trimmed_value);

  return form_group->SetInfoWithVerificationStatusImpl(type, trimmed_value,
                                                       app_locale, status);
}

// static
void AutofillProfile::CreateInferredLabelsHelper(
    const std::vector<AutofillProfile*>& profiles,
    const std::list<size_t>& indices,
    const std::vector<ServerFieldType>& fields,
    size_t num_fields_to_include,
    const std::string& app_locale,
    std::vector<std::u16string>* labels) {
  // For efficiency, we first construct a map of fields to their text values and
  // each value's frequency.
  std::map<ServerFieldType, std::map<std::u16string, size_t>>
      field_text_frequencies_by_field;
  for (const ServerFieldType& field : fields) {
    std::map<std::u16string, size_t>& field_text_frequencies =
        field_text_frequencies_by_field[field];

    for (const auto& it : indices) {
      const AutofillProfile* profile = profiles[it];
      std::u16string field_text =
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
    for (auto field : fields) {
      // Skip over empty fields.
      std::u16string field_text =
          profile->GetInfo(AutofillType(field), app_locale);
      if (field_text.empty())
        continue;

      std::map<std::u16string, size_t>& field_text_frequencies =
          field_text_frequencies_by_field[field];
      found_differentiating_field |=
          !field_text_frequencies.count(std::u16string()) &&
          (field_text_frequencies[field_text] == 1);

      // Once we've found enough non-empty fields, skip over any remaining
      // fields that are identical across all the profiles.
      if (label_fields.size() >= num_fields_to_include &&
          (field_text_frequencies.size() == 1))
        continue;

      label_fields.push_back(field);

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

const FormGroup* AutofillProfile::FormGroupForType(
    const AutofillType& type) const {
  return const_cast<AutofillProfile*>(this)->MutableFormGroupForType(type);
}

FormGroup* AutofillProfile::MutableFormGroupForType(const AutofillType& type) {
  switch (type.group()) {
    case FieldTypeGroup::kName:
    case FieldTypeGroup::kNameBilling:
      return &name_;

    case FieldTypeGroup::kEmail:
      return &email_;

    case FieldTypeGroup::kCompany:
      return &company_;

    case FieldTypeGroup::kPhoneHome:
    case FieldTypeGroup::kPhoneBilling:
      return &phone_number_;

    case FieldTypeGroup::kAddressHome:
    case FieldTypeGroup::kAddressBilling:
      return &address_;

    case FieldTypeGroup::kBirthdateField:
      return &birthdate_;

    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kIban:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUnfillable:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

bool AutofillProfile::EqualsSansGuid(const AutofillProfile& profile) const {
  return origin() == profile.origin() &&
         disallow_settings_visible_updates() ==
             profile.disallow_settings_visible_updates() &&
         language_code() == profile.language_code() &&
         profile_label() == profile.profile_label() &&
         source() == profile.source() && Compare(profile) == 0;
}

std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile) {
  os << (profile.record_type() == AutofillProfile::LOCAL_PROFILE
             ? profile.guid()
             : base::HexEncode(profile.server_id().data(),
                               profile.server_id().size()))
     << " " << profile.origin() << " "
     << "label: " << profile.profile_label() << " " << profile.has_converted()
     << " " << profile.use_count() << " " << profile.use_date() << " "
     << profile.language_code() << std::endl;

  // Lambda to print the value and verification status for |type|.
  auto print_values_lambda = [&os, &profile](ServerFieldType type) {
    os << AutofillType::ServerFieldTypeToString(type) << ": "
       << profile.GetRawInfo(type) << "(" << profile.GetVerificationStatus(type)
       << ")" << std::endl;
  };

  // Use a helper function to print the values of the stored types.
  const ServerFieldType field_types_to_print[] = {
      NAME_FULL,
      NAME_HONORIFIC_PREFIX,
      NAME_FIRST,
      NAME_MIDDLE,
      NAME_LAST,
      NAME_LAST_FIRST,
      NAME_LAST_CONJUNCTION,
      NAME_LAST_SECOND,
      EMAIL_ADDRESS,
      COMPANY_NAME,
      ADDRESS_HOME_ADDRESS,
      ADDRESS_HOME_LINE1,
      ADDRESS_HOME_LINE2,
      ADDRESS_HOME_LINE3,
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_STREET_NAME,
      ADDRESS_HOME_DEPENDENT_STREET_NAME,
      ADDRESS_HOME_HOUSE_NUMBER,
      ADDRESS_HOME_APT_NUM,
      ADDRESS_HOME_FLOOR,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_PREMISE_NAME,
      ADDRESS_HOME_SUBPREMISE,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
      PHONE_HOME_WHOLE_NUMBER};

  base::ranges::for_each(field_types_to_print, print_values_lambda);

  return os;
}

bool AutofillProfile::FinalizeAfterImport() {
  bool success = true;
  if (!name_.FinalizeAfterImport(IsVerified()))
    success = false;
  if (!address_.FinalizeAfterImport(IsVerified()))
    success = false;

  return success;
}

bool AutofillProfile::HasStructuredData() {
  return base::ranges::any_of(kStructuredDataTypes, [this](auto type) {
    return !this->GetRawInfo(type).empty();
  });
}

AutofillProfile AutofillProfile::ConvertToAccountProfile() const {
  DCHECK_EQ(source(), Source::kLocalOrSyncable);
  AutofillProfile account_profile = *this;
  // Since GUIDs are assumed to be unique across all profile sources, a new GUID
  // is assigned.
  account_profile.set_guid(base::GenerateGUID());
  account_profile.source_ = Source::kAccount;
  // Initial creator and last modifier are unused for kLocalOrSyncable profiles.
  account_profile.initial_creator_id_ = kInitialCreatorOrModifierChrome;
  account_profile.last_modifier_id_ = kInitialCreatorOrModifierChrome;
  return account_profile;
}

ServerFieldTypeSet AutofillProfile::FindInaccessibleProfileValues() const {
  ServerFieldTypeSet inaccessible_fields;
  const std::string stored_country =
      base::UTF16ToUTF8(GetRawInfo(ADDRESS_HOME_COUNTRY));
  AutofillCountry country(stored_country.empty() ? "US" : stored_country);
  // Consider only AddressFields which are invisible in the settings for some
  // countries.
  for (const AddressField& field_type :
       {AddressField::ADMIN_AREA, AddressField::LOCALITY,
        AddressField::DEPENDENT_LOCALITY, AddressField::POSTAL_CODE,
        AddressField::SORTING_CODE}) {
    ServerFieldType server_field_type = i18n::TypeForField(field_type);
    if (HasRawInfo(server_field_type) &&
        !country.IsAddressFieldSettingAccessible(field_type)) {
      inaccessible_fields.insert(server_field_type);
    }
  }
  return inaccessible_fields;
}

void AutofillProfile::ClearFields(const ServerFieldTypeSet& fields) {
  for (ServerFieldType server_field_type : fields) {
    SetRawInfoWithVerificationStatus(server_field_type, u"",
                                     VerificationStatus::kNoStatus);
  }
}

}  // namespace autofill
