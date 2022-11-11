// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_INFO_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_INFO_SYNC_UTIL_H_

#include <memory>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/sync/protocol/contact_info_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

// Converts the given `profile` into a `syncer::EntityData` with equivalent
// `ContactInfoSpecifics`.
// Only applicable for profiles with source `kAccount`.
std::unique_ptr<syncer::EntityData>
CreateContactInfoEntityDataFromAutofillProfile(const AutofillProfile& profile);

// Converts the given contact info `specifics` into an equivalent
// `AutofillProfile`.
std::unique_ptr<AutofillProfile> CreateAutofillProfileFromContactInfoSpecifics(
    const sync_pb::ContactInfoSpecifics& specifics);

// Tests if the contact info `specifics` are valid and can be converted into an
// `AutofillProfile` using `CreateAutofillProfileFromContactInfoSpecifics()`.
bool AreContactInfoSpecificsValid(
    const sync_pb::ContactInfoSpecifics& specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PROFILE_SYNC_UTIL_H_
