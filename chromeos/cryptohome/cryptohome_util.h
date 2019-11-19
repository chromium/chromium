// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
#define CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "base/optional.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

// Returns a MountError code from the MountEx |reply| returning
// MOUNT_ERROR_NONE if the reply is well-formed and there is no error.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
MountError MountExReplyToMountError(const base::Optional<BaseReply>& reply);

// Returns a MountError code from |reply|, returning MOUNT_ERROR_NONE
// if the reply is well-formed and there is no error.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
MountError BaseReplyToMountError(const base::Optional<BaseReply>& reply);

// Returns a MountError code from the GetKeyDataEx |reply| returning
// MOUNT_ERROR_NONE if the reply is well-formed and there is no error.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
MountError GetKeyDataReplyToMountError(const base::Optional<BaseReply>& reply);

COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
std::vector<KeyDefinition> GetKeyDataReplyToKeyDefinitions(
    const base::Optional<BaseReply>& reply);

// Extracts the account's disk usage size from |reply|.
// If |reply| is malformed, returns -1.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
int64_t AccountDiskUsageReplyToUsageSize(
    const base::Optional<BaseReply>& reply);

// Extracts the mount hash from |reply|.
// This method assumes |reply| is well-formed. To check if a reply
// is well-formed, callers can check if BaseReplyToMountError returns
// MOUNT_ERROR_NONE.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
const std::string& MountExReplyToMountHash(const BaseReply& reply);

// Creates an AuthorizationRequest from the given secret and label.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
AuthorizationRequest CreateAuthorizationRequest(const std::string& label,
                                                const std::string& secret);

// Creates an AuthorizationRequest from the given key definition.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
AuthorizationRequest CreateAuthorizationRequestFromKeyDef(
    const KeyDefinition& key_def);

// Converts the given KeyDefinition to a Key.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
void KeyDefinitionToKey(const KeyDefinition& key_def, Key* key);

// Converts CryptohomeErrorCode to MountError.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
MountError CryptohomeErrorToMountError(CryptohomeErrorCode code);

// Converts the given KeyAuthorizationData to AuthorizationData pointed to by
// |authorization_data|.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
void KeyAuthorizationDataToAuthorizationData(
    const KeyAuthorizationData& authorization_data_proto,
    KeyDefinition::AuthorizationData* authorization_data);

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
