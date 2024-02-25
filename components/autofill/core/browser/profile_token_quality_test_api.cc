// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality_test_api.h"

#include <vector>

#include "base/check.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/profile_token_quality.h"

namespace autofill {

ProfileTokenQualityTestApi::ProfileTokenQualityTestApi(
    ProfileTokenQuality* quality)
    : quality_(*quality) {}

void ProfileTokenQualityTestApi::AddObservation(
    FieldType field_type,
    ProfileTokenQuality::ObservationType observation_type) {
  AddObservation(field_type, observation_type, FormSignatureHash(0));
}

void ProfileTokenQualityTestApi::AddObservation(
    FieldType field_type,
    ProfileTokenQuality::ObservationType observation_type,
    FormSignatureHash hash) {
  quality_->AddObservation(
      field_type,
      ProfileTokenQuality::Observation{
          .type = base::to_underlying(observation_type), .form_hash = hash});
}

std::vector<ProfileTokenQualityTestApi::FormSignatureHash>
ProfileTokenQualityTestApi::GetHashesForStoredType(FieldType type) const {
  CHECK(GetDatabaseStoredTypesOfAutofillProfile().contains(type));
  auto it = quality_->observations_.find(type);
  if (it == quality_->observations_.end()) {
    return {};
  }
  std::vector<FormSignatureHash> hashes;
  for (const ProfileTokenQuality::Observation& observation : it->second) {
    hashes.push_back(observation.form_hash);
  }
  return hashes;
}

}  // namespace autofill
