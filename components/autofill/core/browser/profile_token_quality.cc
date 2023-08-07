// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality.h"

#include <algorithm>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_features.h"
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
  if (ProfileTokenQuality::IsStoredType(type)) {
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

// Returns true if `a` and `b` differ by Levenshtein distance of at most `k`.
// Edits, inserts and removes each count as one step.
// Runs in O(|a| * k) time and O(k) memory.
bool IsWithinLevenshteinDistance(std::u16string_view a,
                                 std::u16string_view b,
                                 size_t k) {
  // If the string's lengths differ by more than `k`, so does their
  // Levenshtein distance.
  if (a.size() + k < b.size() || a.size() > b.size() + k) {
    return false;
  }
  // The classical Levenshtein distance DP defines dp[i][j] as the edit distance
  // of a[:i] and b[:j]. To make this more efficient, one can define dp[i][d] as
  // the edit-distance of a[:i] and b[:i + d]. Intuitively, d represents the
  // delta between j and i in the former dp. Since the edit-distance is
  // restricted by `k`, abs(d) can be bounded by `k`.
  // Since dp[i][d] only depends on values from dp[i-1], it is not necessary to
  // store the entire 2D table. Instead, this code just stores the d-dimension,
  // which represents "the edit-distance with the current prefix of the string,
  // for a given delta d".
  // Since d is between `-k` and `k`, the implementation shifts the d-index by
  // `k`, bringing it in range [0, `2*k`]

  // The algorithm only cares if the Levenshtein distance is at most `k`. Thus,
  // any unreachable states and states in which the edit-distance is certainly
  // larger than `k` can be set to any value larger than `k`, without affecting
  // the result.
  const size_t kInfinity = k + 1;
  std::vector<size_t> dp(2 * k + 1, kInfinity);
  // Initially, `dp[d]` represents the Levenshtein distance of the empty prefix
  // of `a` and the j = d - k characters of `b`. Their edit distance is j, since
  // j removals are required. States with negative d are not reachable, since
  // that corresponds to a negative index into `b`.
  std::iota(dp.begin() + k, dp.end(), 0);
  for (size_t i = 0; i < a.size(); i++) {
    // Right now, `dp` represents the Levenshtein distance when considering the
    // first `i` characters (up to index `i-1`) of `a`. After the next loop,
    // `dp` will represent the Levenshtein distance when considering the first
    // `i+1` characters.
    for (size_t d = 0; d <= 2 * k; d++) {
      if (i + d < k || i + d >= b.size() + k) {
        // `j = i + d - k` is out of range of `b`.
        dp[d] = kInfinity;
        continue;
      }
      const size_t j = i + d - k;
      // If `a[i] == `b[j]` the Levenshtein distance for `d` remained the same.
      if (a[i] != b[j]) {
        // (i, j) -> (i-1, j-1), `d` stays the same.
        const size_t replace = dp[d];
        // (i, j) -> (i-1, j), `d` increases by 1.
        // If the distance between `i` and `j` becomes larger than `k`, their
        // edit distance is at least `k + 1`. Same in the `insert` case.
        const size_t remove = d != 2 * k ? dp[d + 1] : kInfinity;
        // (i, j) -> (i, j-1), `d` decreases by 1. Since `i` stays the same,
        // this is intentionally using the dp value updated in the previous
        // iteration.
        const size_t insert = d != 0 ? dp[d - 1] : kInfinity;
        dp[d] = 1 + std::min({replace, remove, insert});
      }
    }
  }
  return dp[b.size() + k - a.size()] <= k;
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

  if (IsWithinLevenshteinDistance(
          profile.GetInfo(type, app_locale), edited_value,
          ProfileTokenQuality::kMaximumLevenshteinDistance)) {
    return ObservationType::kEditedToSimilarValue;
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

  return ObservationType::kEditedFallback;
}

}  // namespace

ProfileTokenQuality::ProfileTokenQuality(AutofillProfile* profile)
    : profile_(profile) {
  CHECK(profile);
}

ProfileTokenQuality::ProfileTokenQuality(const ProfileTokenQuality& other) =
    default;

ProfileTokenQuality::~ProfileTokenQuality() = default;

// static
bool ProfileTokenQuality::IsStoredType(ServerFieldType type) {
  return base::Contains(AutofillTable::GetStoredTypesForAutofillProfile(),
                        type);
}

bool ProfileTokenQuality::AddObservationsForFilledForm(
    const FormStructure& form_structure,
    const FormData& form_data,
    const PersonalDataManager& pdm) {
  CHECK_EQ(form_structure.field_count(), form_data.fields.size());

  std::vector<AutofillProfile*> other_profiles = pdm.GetProfiles();
  base::EraseIf(other_profiles, [&](AutofillProfile* p) {
    return p->guid() == profile_->guid();
  });

  std::vector<std::pair<ServerFieldType, Observation>> possible_observations;
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
    possible_observations.emplace_back(
        stored_type, Observation{.type = GetObservationTypeFromField(
                                     field, form_data.fields[i].value,
                                     other_profiles, pdm.app_locale()),
                                 .form_hash = hash});
  }
  return AddSubsetOfObservations(std::move(possible_observations)) > 0;
}

// static
void ProfileTokenQuality::SaveObservationsForFilledFormForAllSubmittedProfiles(
    const FormStructure& form_structure,
    const FormData& form_data,
    PersonalDataManager& pdm) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillTrackProfileTokenQuality)) {
    return;
  }

  std::set<std::string> guids_seen;
  for (const std::unique_ptr<AutofillField>& field : form_structure) {
    if (!field->autofill_source_profile_guid() ||
        !guids_seen.insert(*field->autofill_source_profile_guid()).second) {
      // The field was not autofilled or observations were already collected
      // for the profile that was used to autofill the field.
      continue;
    }
    AutofillProfile* profile =
        pdm.GetProfileByGUID(*field->autofill_source_profile_guid());
    if (profile && profile->token_quality().AddObservationsForFilledForm(
                       form_structure, form_data, pdm)) {
      // TODO(crbug.com/1453650): Update `*profile` in the database, once
      // `AutofillTable` supports storing token quality.
    }
  }
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

// static
bool ProfileTokenQuality::IsWithinLevenshteinDistanceForTesting(
    std::u16string_view a,
    std::u16string_view b,
    size_t k) {
  return IsWithinLevenshteinDistance(a, b, k);
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

size_t ProfileTokenQuality::AddSubsetOfObservations(
    std::vector<std::pair<ServerFieldType, Observation>> observations) {
  if (observations.empty()) {
    return 0;
  }
  const size_t observations_to_add =
      diable_randomization_for_testing_ ? observations.size()
      : observations_.size() >= 11      ? 8
      : observations.size() > 3         ? observations.size() - 3
                                        : 1;
  // Shuffle the `observations` and add the first `observations_to_add` many.
  base::RandomShuffle(observations.begin(), observations.end());
  for (auto& [type, observation] : base::make_span(
           observations.begin(), observations.begin() + observations_to_add)) {
    AddObservation(type, std::move(observation));
  }
  return observations_to_add;
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
    return IsStoredType(type) ? ObservationType::kAccepted
                              : ObservationType::kPartiallyAccepted;
  }

  // Since the `autofill_source_profile_guid()` is set and the field is not
  // autofilled anymore, it must have been previously autofilled.
  CHECK(field.previously_autofilled());
  return GetObservationTypeForEditedField(type, current_field_value, *profile_,
                                          other_profiles, app_locale);
}

void ProfileTokenQuality::CopyObservationsForStoredType(
    ServerFieldType type,
    const ProfileTokenQuality& other) {
  CHECK(IsStoredType(type));
  if (auto it = other.observations_.find(type);
      it != other.observations_.end()) {
    observations_[type] = it->second;
  } else {
    ResetObservationsForStoredType(type);
  }
}

void ProfileTokenQuality::ResetObservationsForStoredType(ServerFieldType type) {
  CHECK(IsStoredType(type));
  observations_.erase(type);
}

ProfileTokenQuality::FormSignatureHash
ProfileTokenQuality::GetFormSignatureHash(FormSignature form_signature) const {
  // Just take the lowest 8 bits of the `form_signature`.
  static_assert(sizeof(FormSignatureHash) == 1);
  return FormSignatureHash(form_signature.value());
}

}  // namespace autofill
