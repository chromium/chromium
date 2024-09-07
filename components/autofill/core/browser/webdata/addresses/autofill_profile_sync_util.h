// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_AUTOFILL_PROFILE_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_AUTOFILL_PROFILE_SYNC_UTIL_H_

#include <memory>
#include <optional>
#include <string>

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace sync_pb {
class AutofillProfileSpecifics;
}  // namespace sync_pb

namespace autofill {

class AutofillProfile;

// Converts the given `entry` into a syncer EntityData with equivalent
// autofill profile specifics. Returns nullptr if `entry` is invalid.
// Shortens all string fields to AutofillTable::kMaxDataLength.
// Only applicable for profiles with source `kLocalOrSyncable`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromAutofillProfile(
    const AutofillProfile& entry);

// Converts the given autofill profile |specifics| into an equivalent
// AutofillProfile. Returns nullptr if |specifics| is invalid.
std::optional<AutofillProfile> CreateAutofillProfileFromSpecifics(
    const sync_pb::AutofillProfileSpecifics& specifics);

// Returns the storage key for given |entry|, to be used for storing in the
// database. Returns an empty string if |entry| is invalid.
std::string GetStorageKeyFromAutofillProfile(const AutofillProfile& entry);

// Returns the storage key for given |specifics|, to be used for storing in the
// database. Returns an empty string if |entry| is invalid.
std::string GetStorageKeyFromAutofillProfileSpecifics(
    const sync_pb::AutofillProfileSpecifics& specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_AUTOFILL_PROFILE_SYNC_UTIL_H_
