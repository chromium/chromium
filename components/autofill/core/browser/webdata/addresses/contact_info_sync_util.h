// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_SYNC_UTIL_H_

#include <memory>
#include <optional>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/sync/protocol/contact_info_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

// For a given `profile`, returns the corresponding
// `sync_pb::ContactInfoSpecifics` based on top of potentially remaining
// unsupported fields that are preserved in `base_contact_info_specifics`.
// Unsupported fields are fields that have been added to the protobuf definition
// in a version newer than the one used in a specific client version.
sync_pb::ContactInfoSpecifics ContactInfoSpecificsFromAutofillProfile(
    const AutofillProfile& profile,
    const sync_pb::ContactInfoSpecifics& base_contact_info_specifics);

// Converts the given `profile` into a `syncer::EntityData` with equivalent
// `ContactInfoSpecifics`. The data from `profile` is written on top of
// `base_contact_info_specifics` to allow preserving unsupported fields.
// Unsupported fields are fields that have been added to the protobuf definition
// in a version newer than the one used in a specific client version.
// Only applicable for profiles with source `kAccount`.
std::unique_ptr<syncer::EntityData>
CreateContactInfoEntityDataFromAutofillProfile(
    const AutofillProfile& profile,
    const sync_pb::ContactInfoSpecifics& base_contact_info_specifics);

// Converts the given contact info `specifics` into an equivalent
// `AutofillProfile`.
std::optional<AutofillProfile> CreateAutofillProfileFromContactInfoSpecifics(
    const sync_pb::ContactInfoSpecifics& specifics);

// Tests if the contact info `specifics` are valid and can be converted into an
// `AutofillProfile` using `CreateAutofillProfileFromContactInfoSpecifics()`.
bool AreContactInfoSpecificsValid(
    const sync_pb::ContactInfoSpecifics& specifics);

// Clears all supported fields from `contact_info_specifics`. Supported fields
// are all fields in the protobuf definition that have already been included in
// the client version.
sync_pb::ContactInfoSpecifics TrimContactInfoSpecificsDataForCaching(
    const sync_pb::ContactInfoSpecifics& contact_info_specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_SYNC_UTIL_H_
