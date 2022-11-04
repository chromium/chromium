// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_
#define COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_

#include "components/sync/protocol/nigori_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {

// The different states for the encryption passphrase. These control if and how
// the user should be prompted for a decryption passphrase.
// Do not re-order or delete these entries; they are used in a UMA histogram.
// Please edit SyncPassphraseType in enums.xml if a value is added.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync
enum class PassphraseType {
  // GAIA-based passphrase (deprecated).
  // TODO(crbug.com/1201684): Some codepaths use this value as a synonym for
  // an unknown passphrase type. Rename to reflect this or use absl::optional<>.
  kImplicitPassphrase = 0,
  // Keystore passphrase.
  kKeystorePassphrase = 1,
  // Frozen GAIA passphrase.
  kFrozenImplicitPassphrase = 2,
  // User-provided passphrase.
  kCustomPassphrase = 3,
  // Trusted-vault passphrase.
  kTrustedVaultPassphrase = 4,
  // Alias used by UMA macros to deduce the correct boundary value.
  kMaxValue = kTrustedVaultPassphrase
};

bool IsExplicitPassphrase(PassphraseType type);

// Function meant to convert |NigoriSpecifics.passphrase_type| into enum.
// All unknown values are returned as UNKNOWN, which mainly represents future
// values of the enum that are not currently supported. Note however that if the
// field is not populated, it defaults to IMPLICIT_PASSPHRASE for backwards
// compatibility reasons.
sync_pb::NigoriSpecifics::PassphraseType ProtoPassphraseInt32ToProtoEnum(
    ::google::protobuf::int32 type);

// Returns absl::nullopt if |type| represents an unknown value, likely coming
// from a future version of the browser. Note however that if the field is not
// populated, it defaults to IMPLICIT_PASSPHRASE for backwards compatibility
// reasons.
absl::optional<PassphraseType> ProtoPassphraseInt32ToEnum(
    ::google::protobuf::int32 type);

sync_pb::NigoriSpecifics::PassphraseType EnumPassphraseTypeToProto(
    PassphraseType type);

// Different key derivation methods. Used for deriving the encryption key from a
// user's custom passphrase.
enum class KeyDerivationMethod {
  PBKDF2_HMAC_SHA1_1003 = 0,  // PBKDF2-HMAC-SHA1 with 1003 iterations.
  SCRYPT_8192_8_11 = 1,  // scrypt with N = 2^13, r = 8, p = 11 and random salt.
};

// This function accepts an integer and not KeyDerivationMethod from the proto
// in order to be able to handle new, unknown values. Returns nullopt if value
// is unknown (indicates protocol violation or value coming from newer version)
// and PBKDF2_HMAC_SHA1_1003 if value is unspecified (indicates value coming
// from older version, that is not aware of the field).
absl::optional<KeyDerivationMethod> ProtoKeyDerivationMethodToEnum(
    ::google::protobuf::int32 method);

sync_pb::NigoriSpecifics::KeyDerivationMethod EnumKeyDerivationMethodToProto(
    KeyDerivationMethod method);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_
