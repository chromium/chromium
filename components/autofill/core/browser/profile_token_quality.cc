// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"

namespace autofill {

namespace {

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
  NOTIMPLEMENTED();
  return false;
}

void ProfileTokenQuality::AddObservationForTesting(
    ServerFieldType field_type,
    ObservationType observation_type) {
  AddObservation(field_type, Observation{.type = observation_type});
}

std::vector<ProfileTokenQuality::ObservationType>
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

ProfileTokenQuality::FormSignatureHash
ProfileTokenQuality::GetFormSignatureHash(FormSignature form_signature) const {
  // Just take the lowest 8 bits of the `form_signature`.
  static_assert(sizeof(FormSignatureHash) == 1);
  return FormSignatureHash(form_signature.value());
}

}  // namespace autofill
