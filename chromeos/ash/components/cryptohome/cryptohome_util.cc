// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/cryptohome_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

namespace {

using ::ash::ChallengeResponseKey;
using ::google::protobuf::RepeatedPtrField;

ChallengeSignatureAlgorithm ChallengeSignatureAlgorithmToProtoEnum(
    ChallengeResponseKey::SignatureAlgorithm algorithm) {
  using Algorithm = ChallengeResponseKey::SignatureAlgorithm;
  switch (algorithm) {
    case Algorithm::kRsassaPkcs1V15Sha1:
      return CHALLENGE_RSASSA_PKCS1_V1_5_SHA1;
    case Algorithm::kRsassaPkcs1V15Sha256:
      return CHALLENGE_RSASSA_PKCS1_V1_5_SHA256;
    case Algorithm::kRsassaPkcs1V15Sha384:
      return CHALLENGE_RSASSA_PKCS1_V1_5_SHA384;
    case Algorithm::kRsassaPkcs1V15Sha512:
      return CHALLENGE_RSASSA_PKCS1_V1_5_SHA512;
  }
  NOTREACHED();
}

void ChallengeResponseKeyToPublicKeyInfo(
    const ChallengeResponseKey& challenge_response_key,
    ChallengePublicKeyInfo* public_key_info) {
  public_key_info->set_public_key_spki_der(
      challenge_response_key.public_key_spki_der());
  for (ChallengeResponseKey::SignatureAlgorithm algorithm :
       challenge_response_key.signature_algorithms()) {
    public_key_info->add_signature_algorithm(
        ChallengeSignatureAlgorithmToProtoEnum(algorithm));
  }
}

void KeyDefPrivilegesToKeyPrivileges(int key_def_privileges,
                                     KeyPrivileges* privileges) {
  privileges->set_add(key_def_privileges & PRIV_ADD);
  privileges->set_remove(key_def_privileges & PRIV_REMOVE);
  privileges->set_update(key_def_privileges & PRIV_MIGRATE);
}

// TODO(crbug.com/797848): Add tests that cover this logic.
void KeyDefProviderDataToKeyProviderDataEntry(
    const KeyDefinition::ProviderData& provider_data,
    KeyProviderData::Entry* entry) {
  entry->set_name(provider_data.name);
  if (provider_data.number) {
    entry->set_number(*provider_data.number);
  }

  if (provider_data.bytes) {
    entry->set_bytes(*provider_data.bytes);
  }
}

}  // namespace

KeyDefinition KeyDataToKeyDefinition(const KeyData& key_data) {
  CHECK(key_data.has_type());
  KeyDefinition result;
  // Extract |type|, |label| and |revision|.
  switch (key_data.type()) {
    case KeyData::KEY_TYPE_PASSWORD:
      result.type = KeyDefinition::TYPE_PASSWORD;
      break;
    case KeyData::KEY_TYPE_CHALLENGE_RESPONSE:
      result.type = KeyDefinition::TYPE_CHALLENGE_RESPONSE;
      break;
    case KeyData::KEY_TYPE_FINGERPRINT:
      // KEY_TYPE_FINGERPRINT means the key is a request for fingerprint auth
      // and does not really carry any auth information. KEY_TYPE_FINGERPRINT
      // is not expected to be used in GetKeyData.
      NOTREACHED();
      break;
    case KeyData::KEY_TYPE_KIOSK:
      result.type = KeyDefinition::TYPE_PUBLIC_MOUNT;
      break;
  }
  result.label = KeyLabel(key_data.label());
  result.revision = key_data.revision();

  // Extract |privileges|.
  const KeyPrivileges& privileges = key_data.privileges();
  if (privileges.add()) {
    result.privileges |= PRIV_ADD;
  }
  if (privileges.remove()) {
    result.privileges |= PRIV_REMOVE;
  }
  if (privileges.update()) {
    result.privileges |= PRIV_MIGRATE;
  }

  // Extract |policy|.
  result.policy.low_entropy_credential =
      key_data.policy().low_entropy_credential();
  result.policy.auth_locked = key_data.policy().auth_locked();

  // Extract |provider_data|.
  for (auto& provider_datum : key_data.provider_data().entry()) {
    // We have either of two
    DCHECK_NE(provider_datum.has_number(), provider_datum.has_bytes());

    if (provider_datum.has_number()) {
      result.provider_data.push_back(KeyDefinition::ProviderData(
          provider_datum.name(), provider_datum.number()));
    } else {
      result.provider_data.push_back(KeyDefinition::ProviderData(
          provider_datum.name(), provider_datum.bytes()));
    }
  }
  return result;
}

std::vector<KeyDefinition> RepeatedKeyDataToKeyDefinitions(
    const RepeatedPtrField<KeyData>& key_data) {
  std::vector<KeyDefinition> key_definitions;
  for (RepeatedPtrField<KeyData>::const_iterator it = key_data.begin();
       it != key_data.end(); ++it) {
    key_definitions.push_back(KeyDataToKeyDefinition(*it));
  }
  return key_definitions;
}

AuthorizationRequest CreateAuthorizationRequest(const KeyLabel& label,
                                                const std::string& secret) {
  return CreateAuthorizationRequestFromKeyDef(
      KeyDefinition::CreateForPassword(secret, label, PRIV_DEFAULT));
}

AuthorizationRequest CreateAuthorizationRequestFromKeyDef(
    const KeyDefinition& key_def) {
  cryptohome::AuthorizationRequest auth_request;
  KeyDefinitionToKey(key_def, auth_request.mutable_key());

  switch (key_def.type) {
    case KeyDefinition::TYPE_PASSWORD:
      break;
    case KeyDefinition::TYPE_CHALLENGE_RESPONSE:
      // Specify the additional KeyDelegate information that allows cryptohomed
      // to call back to Chrome to perform cryptographic challenges.
      auth_request.mutable_key_delegate()->set_dbus_service_name(
          cryptohome::kCryptohomeKeyDelegateServiceName);
      auth_request.mutable_key_delegate()->set_dbus_object_path(
          cryptohome::kCryptohomeKeyDelegateServicePath);
      break;
    case KeyDefinition::TYPE_FINGERPRINT:
      break;
    case KeyDefinition::TYPE_PUBLIC_MOUNT:
      break;
  }

  return auth_request;
}

// TODO(crbug.com/797848): Finish testing this method.
void KeyDefinitionToKey(const KeyDefinition& key_def, Key* key) {
  KeyData* data = key->mutable_data();
  if (!key_def.label.value().empty()) {
    data->set_label(key_def.label.value());
  }

  switch (key_def.type) {
    case KeyDefinition::TYPE_PASSWORD:
      data->set_type(KeyData::KEY_TYPE_PASSWORD);
      key->set_secret(key_def.secret);
      break;

    case KeyDefinition::TYPE_CHALLENGE_RESPONSE:
      data->set_type(KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
      for (const auto& challenge_response_key :
           key_def.challenge_response_keys) {
        ChallengeResponseKeyToPublicKeyInfo(challenge_response_key,
                                            data->add_challenge_response_key());
      }
      break;

    case KeyDefinition::TYPE_FINGERPRINT:
      data->set_type(KeyData::KEY_TYPE_FINGERPRINT);
      break;
    case KeyDefinition::TYPE_PUBLIC_MOUNT:
      data->set_type(KeyData::KEY_TYPE_KIOSK);
      break;
  }

  if (key_def.revision > 0) {
    data->set_revision(key_def.revision);
  }

  if (key_def.privileges != 0) {
    KeyDefPrivilegesToKeyPrivileges(key_def.privileges,
                                    data->mutable_privileges());
  }

  for (const auto& provider_data : key_def.provider_data) {
    KeyDefProviderDataToKeyProviderDataEntry(
        provider_data, data->mutable_provider_data()->add_entry());
  }
}

}  // namespace cryptohome
