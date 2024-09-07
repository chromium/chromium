// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality.h"

#include <algorithm>
#include <set>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/levenshtein_distance.h"
#include "base/strings/string_util.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

namespace {

using ObservationType = ProfileTokenQuality::ObservationType;

FieldTypeSet GetSupportedTypes(const AutofillProfile& profile) {
  FieldTypeSet types;
  profile.GetSupportedTypes(&types);
  return types;
}

// Computes the `ObservationType` if a field of the given `type` was autofilled
// with the `profile`, but the autofilled value was edited to `edited_value`
// after filling.
ObservationType GetObservationTypeForEditedField(
    FieldType type,
    std::u16string_view edited_value,
    const AutofillProfile& profile,
    const std::vector<const AutofillProfile*>& other_profiles,
    const std::string& app_locale) {
  if (edited_value.empty()) {
    return ObservationType::kEditedValueCleared;
  }

  if (base::LevenshteinDistance(
          base::ToLowerASCII(profile.GetInfo(type, app_locale)),
          base::ToLowerASCII(edited_value),
          ProfileTokenQuality::kMaximumLevenshteinDistance) <=
      ProfileTokenQuality::kMaximumLevenshteinDistance) {
    return ObservationType::kEditedToSimilarValue;
  }

  // Returns true if the `current_field_value` case-insensitively equals the
  // value of the `profile` for any of the `types`.
  auto matches = [&](FieldTypeSet types, const AutofillProfile& profile) {
    const l10n::CaseInsensitiveCompare compare;
    return std::ranges::any_of(types, [&](FieldType type) {
      return profile.HasInfo(type) &&
             compare.StringsEqual(edited_value,
                                  profile.GetInfo(type, app_locale));
    });
  };

  // Returns all supported types of the `profile` except for `type`.
  auto other_types = [&](const AutofillProfile& profile) {
    FieldTypeSet other_types = GetSupportedTypes(profile);
    other_types.erase(type);
    return other_types;
  };

  if (matches(other_types(profile), profile)) {
    return ObservationType::kEditedToDifferentTokenOfSameProfile;
  }

  if (std::ranges::any_of(
          other_profiles, [&](const AutofillProfile* other_profile) {
            return matches(other_types(*other_profile), *other_profile);
          })) {
    return ObservationType::kEditedToDifferentTokenOfOtherProfile;
  }

  if (std::ranges::any_of(other_profiles,
                          [&](const AutofillProfile* other_profile) {
                            return matches({type}, *other_profile);
                          })) {
    return ObservationType::kEditedToSameTokenOfOtherProfile;
  }

  return ObservationType::kEditedFallback;
}

}  // namespace

ProfileTokenQuality::ProfileTokenQuality(AutofillProfile* profile)
    : profile_(CHECK_DEREF(profile)) {}

ProfileTokenQuality::ProfileTokenQuality(const ProfileTokenQuality& other) =
    default;

ProfileTokenQuality::~ProfileTokenQuality() = default;

bool ProfileTokenQuality::operator==(const ProfileTokenQuality& other) const {
  if (profile_->guid() != other.profile_->guid() ||
      observations_.size() != other.observations_.size()) {
    return false;
  }
  // Element-wise comparison between `observations_` and `other.observations_`.
  // base::circular_deque<> intentionally doesn't define a comparison operator.
  using map_entry_t = std::pair<FieldType, base::circular_deque<Observation>>;
  return base::ranges::equal(observations_, other.observations_,
                             [](const map_entry_t& a, const map_entry_t& b) {
                               return a.first == b.first &&
                                      base::ranges::equal(a.second, b.second);
                             });
}

bool ProfileTokenQuality::AddObservationsForFilledForm(
    const FormStructure& form_structure,
    const FormData& form_data,
    const PersonalDataManager& pdm) {
  CHECK_EQ(form_structure.field_count(), form_data.fields().size());

  std::vector<const AutofillProfile*> other_profiles =
      pdm.address_data_manager().GetProfiles();
  std::erase_if(other_profiles, [&](const AutofillProfile* p) {
    return p->guid() == profile_->guid();
  });

  const FieldTypeSet supported_types = GetSupportedTypes(*profile_);
  std::vector<std::pair<FieldType, Observation>> possible_observations;
  for (size_t i = 0; i < form_structure.field_count(); i++) {
    const AutofillField& field = *form_structure.field(i);
    if (field.autofill_source_profile_guid() != profile_->guid()) {
      // The field was not autofilled or autofilled with a different profile.
      continue;
    }
    if (!field.autofilled_type()) {
      // TODO(crbug.com/311604770): Field-by-field filling doesn't support
      // `autofilled_type()`.
      continue;
    }

    const FieldType stored_type =
        profile_->GetStorableTypeOf(*field.autofilled_type());
    if (!supported_types.contains(stored_type)) {
      // If the user changed the country of their profile before submission, the
      // type might not be supported anymore.
      continue;
    }

    const FormSignatureHash hash =
        GetFormSignatureHash(form_structure.form_signature());
    if (auto observations = observations_.find(stored_type);
        observations != observations_.end() &&
        base::Contains(observations->second, hash,
                       [](const Observation& o) { return o.form_hash; })) {
      // An observation for the `stored_type` and `hash` was already collected.
      continue;
    }

    // If the field has a selected option, we give precedence to the option's
    // text over its value because the user-visible text is likely more
    // meaningful. Currently, only <select> elements may have a selected option.
    base::optional_ref<const SelectOption> selected_option =
        form_data.fields()[i].selected_option();
    std::u16string value =
        selected_option ? selected_option->text : form_data.fields()[i].value();
    possible_observations.emplace_back(
        stored_type,
        Observation{.type = base::to_underlying(GetObservationTypeFromField(
                        field, value, other_profiles, pdm.app_locale())),
                    .form_hash = hash});
  }
  return AddSubsetOfObservations(std::move(possible_observations)) > 0;
}

// static
void ProfileTokenQuality::SaveObservationsForFilledFormForAllSubmittedProfiles(
    const FormStructure& form_structure,
    const FormData& form_data,
    PersonalDataManager& pdm) {
  autofill_metrics::LogObservationCountBeforeSubmissionMetric(form_structure,
                                                              pdm);

  std::set<std::string> guids_seen;
  for (const std::unique_ptr<AutofillField>& field : form_structure) {
    if (!field->autofill_source_profile_guid() ||
        !guids_seen.insert(*field->autofill_source_profile_guid()).second) {
      // The field was not autofilled or observations were already collected
      // for the profile that was used to autofill the field.
      continue;
    }
    const AutofillProfile* profile =
        pdm.address_data_manager().GetProfileByGUID(
            *field->autofill_source_profile_guid());
    if (!profile) {
      continue;
    }
    AutofillProfile updatable_profile = *profile;
    if (updatable_profile.token_quality().AddObservationsForFilledForm(
            form_structure, form_data, pdm)) {
      pdm.address_data_manager().UpdateProfile(updatable_profile);
    }
  }

  autofill_metrics::LogProfileTokenQualityScoreMetric(form_structure, pdm);
}

std::vector<ObservationType>
ProfileTokenQuality::GetObservationTypesForFieldType(FieldType type) const {
  const auto it = observations_.find(profile_->GetStorableTypeOf(type));
  if (it == observations_.end()) {
    return {};
  }
  std::vector<ObservationType> types;
  types.reserve(it->second.size());
  for (const Observation& observation : it->second) {
    if (observation.type <= base::to_underlying(ObservationType::kMaxValue)) {
      types.push_back(static_cast<ObservationType>(observation.type));
    } else {
      // This is possible if the `observation.type` was synced from a newer
      // client that supports additional `ObservationType`s that this client
      // doesn't understand.
      types.push_back(ObservationType::kUnknown);
    }
  }
  return types;
}

void ProfileTokenQuality::AddObservation(FieldType type,
                                         Observation observation) {
  CHECK_NE(observation.type, base::to_underlying(ObservationType::kUnknown));
  base::circular_deque<Observation>& observations =
      observations_[profile_->GetStorableTypeOf(type)];
  CHECK_LE(observations.size(), kMaxObservationsPerToken);
  static_assert(kMaxObservationsPerToken > 0);
  if (observations.size() == kMaxObservationsPerToken) {
    observations.pop_front();
  }
  observations.push_back(std::move(observation));
}

size_t ProfileTokenQuality::AddSubsetOfObservations(
    std::vector<std::pair<FieldType, Observation>> observations) {
  if (observations.empty()) {
    return 0;
  }
  const size_t observations_to_add =
      diable_randomization_for_testing_ ? observations.size()
      : observations.size() >= 11       ? 8
      : observations.size() > 3         ? observations.size() - 3
                                        : 1;
  // Shuffle the `observations` and add the first `observations_to_add` many.
  base::RandomShuffle(observations.begin(), observations.end());
  for (auto& [type, observation] :
       base::span(observations).first(observations_to_add)) {
    AddObservation(type, std::move(observation));
  }
  return observations_to_add;
}

ObservationType ProfileTokenQuality::GetObservationTypeFromField(
    const AutofillField& field,
    std::u16string_view current_field_value,
    const std::vector<const AutofillProfile*>& other_profiles,
    const std::string& app_locale) const {
  CHECK(field.autofill_source_profile_guid() == profile_->guid());
  DCHECK(!base::Contains(other_profiles, profile_->guid(),
                         [](const AutofillProfile* p) { return p->guid(); }));

  const FieldType type = field.Type().GetStorableType();
  if (field.is_autofilled()) {
    // The filled value was accepted without editing.
    return GetDatabaseStoredTypesOfAutofillProfile().contains(type)
               ? ObservationType::kAccepted
               : ObservationType::kPartiallyAccepted;
  }

  // Since the `autofill_source_profile_guid()` is set and the field is not
  // autofilled anymore, it must have been previously autofilled.
  return GetObservationTypeForEditedField(type, current_field_value, *profile_,
                                          other_profiles, app_locale);
}

std::vector<uint8_t> ProfileTokenQuality::SerializeObservationsForStoredType(
    FieldType type) const {
  CHECK(GetDatabaseStoredTypesOfAutofillProfile().contains(type));
  std::vector<uint8_t> serialized_data;
  if (auto it = observations_.find(type); it != observations_.end()) {
    for (const Observation& observation : it->second) {
      serialized_data.push_back(observation.type);
      serialized_data.push_back(observation.form_hash.value());
    }
  }
  return serialized_data;
}

void ProfileTokenQuality::LoadSerializedObservationsForStoredType(
    FieldType type,
    base::span<const uint8_t> serialized_data) {
  if (!GetSupportedTypes(*profile_).contains(type)) {
    // Observations only get stored for supported types. However, due to changes
    // in the data model, it is possible for types to become unsupported.
    return;
  }
  // If the database was modified through external means, the `serialized_data`
  // might not be valid. Any invalid entries are skipped.
  for (size_t i = 0; i + 1 < serialized_data.size() &&
                     observations_.size() < kMaxObservationsPerToken;
       i += 2) {
    static_assert(base::to_underlying(ObservationType::kUnknown) == 0);
    if (serialized_data[i] == 0 ||
        serialized_data[i] > base::to_underlying(ObservationType::kMaxValue)) {
      // Invalid data read from disk.
      continue;
    }
    AddObservation(
        type,
        Observation{.type = serialized_data[i],
                    .form_hash = FormSignatureHash(serialized_data[i + 1])});
  }
}

void ProfileTokenQuality::CopyObservationsForStoredType(
    FieldType type,
    const ProfileTokenQuality& other) {
  CHECK(GetDatabaseStoredTypesOfAutofillProfile().contains(type));
  if (auto it = other.observations_.find(type);
      it != other.observations_.end()) {
    observations_[type] = it->second;
  } else {
    ResetObservationsForStoredType(type);
  }
}

void ProfileTokenQuality::ResetObservationsForStoredType(FieldType type) {
  CHECK(GetDatabaseStoredTypesOfAutofillProfile().contains(type));
  observations_.erase(type);
}

void ProfileTokenQuality::ResetObservationsForDifferingTokens(
    const AutofillProfile& other) {
  for (FieldType type : GetDatabaseStoredTypesOfAutofillProfile()) {
    if (profile_->GetRawInfo(type) != other.GetRawInfo(type)) {
      ResetObservationsForStoredType(type);
    }
  }
}

ProfileTokenQuality::FormSignatureHash
ProfileTokenQuality::GetFormSignatureHash(FormSignature form_signature) const {
  // Just take the lowest 8 bits of the `form_signature`.
  static_assert(sizeof(FormSignatureHash) == 1);
  return FormSignatureHash(form_signature.value());
}

}  // namespace autofill
