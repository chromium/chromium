// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/fixed_flat_map.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

namespace {

using ObservationType = ProfileTokenQuality::ObservationType;

ServerFieldTypeSet GetSupportedTypes(const AutofillProfile& profile) {
  ServerFieldTypeSet types;
  profile.GetSupportedTypes(&types);
  return types;
}

// Only a subset of the `GetSupportedTypes()` is stored. Every non-stored type
// is derived from a stored type. This function returns the stored type of
// `type`. If `type` is already a stored type, `type` is returned.
//
// ADDRESS_HOME_ADDRESS is not handled, since it is an artificial, unused type
// to represent the root node of the address tree. The type is not stored and
// not used for filling.
ServerFieldType GetStoredTypeOf(ServerFieldType type) {
  if (base::Contains(AutofillTable::GetStoredTypesForAutofillProfile(), type)) {
    return type;
  }
  CHECK_NE(type, ADDRESS_HOME_ADDRESS);
  static const auto kStoredTypeOf =
      base::MakeFixedFlatMap<ServerFieldType, ServerFieldType>(
          {{ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_ADDRESS},
           {ADDRESS_HOME_LINE2, ADDRESS_HOME_STREET_ADDRESS},
           {ADDRESS_HOME_LINE3, ADDRESS_HOME_STREET_ADDRESS},
           {NAME_MIDDLE_INITIAL, NAME_MIDDLE},
           {PHONE_HOME_NUMBER, PHONE_HOME_WHOLE_NUMBER},
           {PHONE_HOME_CITY_CODE, PHONE_HOME_WHOLE_NUMBER},
           {PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX, PHONE_HOME_WHOLE_NUMBER},
           {PHONE_HOME_COUNTRY_CODE, PHONE_HOME_WHOLE_NUMBER},
           {PHONE_HOME_CITY_AND_NUMBER, PHONE_HOME_WHOLE_NUMBER},
           {PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX,
            PHONE_HOME_WHOLE_NUMBER},
           {PHONE_HOME_NUMBER_PREFIX, PHONE_HOME_WHOLE_NUMBER},
           {PHONE_HOME_NUMBER_SUFFIX, PHONE_HOME_WHOLE_NUMBER}});
  auto* it = kStoredTypeOf.find(type);
  CHECK_NE(it, kStoredTypeOf.end());
  return it->second;
}

// Computes the `ObservationType` if a field of the given `type` was autofilled
// with the `profile`, but the autofilled value was edited to `edited_value`
// after filling.
ObservationType GetObservationTypeForEditedField(
    ServerFieldType type,
    std::u16string_view edited_value,
    const AutofillProfile& profile,
    const std::vector<AutofillProfile*>& other_profiles,
    const std::string& app_locale) {
  if (edited_value.empty()) {
    return ObservationType::kEditedValueCleared;
  }

  // Returns true if the `current_field_value` case-insensitively equals the
  // value of the `profile` for any of the `types`.
  auto matches = [&](ServerFieldTypeSet types, const AutofillProfile& profile) {
    const l10n::CaseInsensitiveCompare compare;
    return base::ranges::any_of(types, [&](ServerFieldType type) {
      return profile.HasInfo(type) &&
             compare.StringsEqual(edited_value,
                                  profile.GetInfo(type, app_locale));
    });
  };

  // Returns all supported types of the `profile` except for `type`.
  auto other_types = [&](const AutofillProfile& profile) {
    ServerFieldTypeSet other_types = GetSupportedTypes(profile);
    other_types.erase(type);
    return other_types;
  };

  if (matches(other_types(profile), profile)) {
    return ObservationType::kEditedToDifferentTokenOfSameProfile;
  }

  if (base::ranges::any_of(other_profiles, [&](AutofillProfile* other_profile) {
        return matches(other_types(*other_profile), *other_profile);
      })) {
    return ObservationType::kEditedToDifferentTokenOfOtherProfile;
  }

  if (base::ranges::any_of(other_profiles, [&](AutofillProfile* other_profile) {
        return matches({type}, *other_profile);
      })) {
    return ObservationType::kEditedToSameTokenOfOtherProfile;
  }

  // TODO(crbug.com/1453650): Handle the `kEditedToSimilarValue` case.
  return ObservationType::kEditedFallback;
}

}  // namespace

ProfileTokenQuality::ProfileTokenQuality(AutofillProfile* profile)
    : profile_(profile) {
  CHECK(profile);
}

ProfileTokenQuality::~ProfileTokenQuality() = default;

bool ProfileTokenQuality::AddObservationsForFilledForm(
    const FormStructure& form_structure,
    const FormData& form_data,
    const PersonalDataManager& pdm) {
  CHECK_EQ(form_structure.field_count(), form_data.fields.size());

  std::vector<AutofillProfile*> other_profiles = pdm.GetProfiles();
  base::EraseIf(other_profiles, [&](AutofillProfile* p) {
    return p->guid() == profile_->guid();
  });

  bool added_observation = false;
  for (size_t i = 0; i < form_structure.field_count(); i++) {
    const AutofillField& field = *form_structure.field(i);
    if (field.autofill_source_profile_guid() != profile_->guid()) {
      // The field was not autofilled or autofilled with a different profile.
      continue;
    }

    const ServerFieldType stored_type =
        GetStoredTypeOf(field.Type().GetStorableType());
    const FormSignatureHash hash =
        GetFormSignatureHash(form_structure.form_signature());
    if (auto observations = observations_.find(stored_type);
        observations != observations_.end() &&
        base::Contains(observations->second, hash,
                       [](const Observation& o) { return o.form_hash; })) {
      // An observation for the `stored_type` and `hash` was already collected.
      continue;
    }
    AddObservation(stored_type,
                   Observation{.type = GetObservationTypeFromField(
                                   field, form_data.fields[i].value,
                                   other_profiles, pdm.app_locale()),
                               .form_hash = hash});
    added_observation = true;
  }
  return added_observation;
}

void ProfileTokenQuality::AddObservationForTesting(
    ServerFieldType field_type,
    ObservationType observation_type) {
  AddObservation(field_type, Observation{.type = observation_type});
}

std::vector<ObservationType>
ProfileTokenQuality::GetObservationTypesForFieldType(
    ServerFieldType type) const {
  CHECK(GetSupportedTypes(*profile_).contains(type));
  const auto it = observations_.find(GetStoredTypeOf(type));
  if (it == observations_.end()) {
    return {};
  }
  std::vector<ObservationType> types;
  types.reserve(it->second.size());
  for (const Observation& observation : it->second) {
    types.push_back(observation.type);
  }
  return types;
}

void ProfileTokenQuality::AddObservation(ServerFieldType type,
                                         Observation observation) {
  CHECK(GetSupportedTypes(*profile_).contains(type));
  CHECK_NE(observation.type, ObservationType::kNone);
  base::circular_deque<Observation>& observations =
      observations_[GetStoredTypeOf(type)];
  CHECK_LE(observations.size(), kMaxObservationsPerToken);
  static_assert(kMaxObservationsPerToken > 0);
  if (observations.size() == kMaxObservationsPerToken) {
    observations.pop_front();
  }
  observations.push_back(std::move(observation));
}

ObservationType ProfileTokenQuality::GetObservationTypeFromField(
    const AutofillField& field,
    std::u16string_view current_field_value,
    const std::vector<AutofillProfile*>& other_profiles,
    const std::string& app_locale) const {
  CHECK(field.autofill_source_profile_guid() == profile_->guid());
  DCHECK(!base::Contains(other_profiles, profile_->guid(),
                         [](AutofillProfile* p) { return p->guid(); }));

  const ServerFieldType type = field.Type().GetStorableType();
  if (field.is_autofilled) {
    // The filled value was accepted without editing.
    return base::Contains(AutofillTable::GetStoredTypesForAutofillProfile(),
                          type)
               ? ObservationType::kAccepted
               : ObservationType::kPartiallyAccepted;
  }

  // Since the `autofill_source_profile_guid()` is set and the field is not
  // autofilled anymore, it must have been previously autofilled.
  CHECK(field.previously_autofilled());
  return GetObservationTypeForEditedField(type, current_field_value, *profile_,
                                          other_profiles, app_locale);
}

ProfileTokenQuality::FormSignatureHash
ProfileTokenQuality::GetFormSignatureHash(FormSignature form_signature) const {
  // Just take the lowest 8 bits of the `form_signature`.
  static_assert(sizeof(FormSignatureHash) == 1);
  return FormSignatureHash(form_signature.value());
}

}  // namespace autofill
