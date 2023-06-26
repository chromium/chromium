// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/profile_token_quality.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

namespace {

ServerFieldTypeSet GetSupportedTypes(const AutofillProfile& profile) {
  ServerFieldTypeSet types;
  profile.GetSupportedTypes(&types);
  return types;
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

std::vector<ProfileTokenQuality::ObservationType>
ProfileTokenQuality::GetObservationTypesForFieldType(
    ServerFieldType type) const {
  CHECK(GetSupportedTypes(*profile_).contains(type));
  NOTIMPLEMENTED();
  return {};
}

ProfileTokenQuality::FormAndFieldSignatureHash
ProfileTokenQuality::GetFormAndFieldSignatureHash(
    FormSignature form_signature,
    FieldSignature field_signature) const {
  NOTIMPLEMENTED();
  return {};
}

}  // namespace autofill
