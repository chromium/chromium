// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/passphrase_enums.h"

#include <optional>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

bool IsExplicitPassphrase(PassphraseType type) {
  switch (type) {
    case PassphraseType::kImplicitPassphrase:
    case PassphraseType::kKeystorePassphrase:
    case PassphraseType::kTrustedVaultPassphrase:
      return false;
    case PassphraseType::kFrozenImplicitPassphrase:
    case PassphraseType::kCustomPassphrase:
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

sync_pb::NigoriSpecifics::PassphraseType ProtoPassphraseInt32ToProtoEnum(
    std::int32_t type) {
  return sync_pb::NigoriSpecifics::PassphraseType_IsValid(type)
             ? static_cast<sync_pb::NigoriSpecifics::PassphraseType>(type)
             : sync_pb::NigoriSpecifics::UNKNOWN;
}

std::optional<PassphraseType> ProtoPassphraseInt32ToEnum(std::int32_t type) {
  switch (ProtoPassphraseInt32ToProtoEnum(type)) {
    case sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE:
      return PassphraseType::kImplicitPassphrase;
    case sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE:
      return PassphraseType::kKeystorePassphrase;
    case sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE:
      return PassphraseType::kCustomPassphrase;
    case sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      return PassphraseType::kFrozenImplicitPassphrase;
    case sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
      return PassphraseType::kTrustedVaultPassphrase;
    case sync_pb::NigoriSpecifics::UNKNOWN:
      // This must be an unknown value coming from future versions or a field
      // actually being populated with UNKNOWN (which is a protocol violation).
      break;
  }

  return std::nullopt;
}

sync_pb::NigoriSpecifics::PassphraseType EnumPassphraseTypeToProto(
    PassphraseType type) {
  switch (type) {
    case PassphraseType::kImplicitPassphrase:
      return sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE;
    case PassphraseType::kKeystorePassphrase:
      return sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE;
    case PassphraseType::kCustomPassphrase:
      return sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE;
    case PassphraseType::kFrozenImplicitPassphrase:
      return sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE;
    case PassphraseType::kTrustedVaultPassphrase:
      return sync_pb::NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE;
  }

  NOTREACHED_IN_MIGRATION();
  return sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE;
}

std::optional<KeyDerivationMethod> ProtoKeyDerivationMethodToEnum(
    std::int32_t method) {
  DCHECK_GE(method, 0);

  switch (method) {
    case sync_pb::NigoriSpecifics::UNSPECIFIED:
      // This is the default value; it comes from an old client (<M70) which
      // does not know about this field. These old clients all use PBKDF2.
      return KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003;
    case sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003;
    case sync_pb::NigoriSpecifics::SCRYPT_8192_8_11:
      return KeyDerivationMethod::SCRYPT_8192_8_11;
  }

  // We do not know about this value. It is likely a method added in a newer
  // version of Chrome.
  return std::nullopt;
}

sync_pb::NigoriSpecifics::KeyDerivationMethod EnumKeyDerivationMethodToProto(
    KeyDerivationMethod method) {
  switch (method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003;
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return sync_pb::NigoriSpecifics::SCRYPT_8192_8_11;
  }

  NOTREACHED_IN_MIGRATION();
  return sync_pb::NigoriSpecifics::UNSPECIFIED;
}

}  // namespace syncer
