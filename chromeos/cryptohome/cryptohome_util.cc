// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/userdataauth_util.h"
#include "chromeos/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::ChallengeResponseKey;
using google::protobuf::RepeatedPtrField;

namespace cryptohome {

namespace {

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
  if (provider_data.number)
    entry->set_number(*provider_data.number);

  if (provider_data.bytes)
    entry->set_bytes(*provider_data.bytes);
}

}  // namespace

std::vector<KeyDefinition> RepeatedKeyDataToKeyDefinitions(
    const RepeatedPtrField<KeyData>& key_data) {
  std::vector<KeyDefinition> key_definitions;
  for (RepeatedPtrField<KeyData>::const_iterator it = key_data.begin();
       it != key_data.end(); ++it) {
    // Extract |type|, |label| and |revision|.
    KeyDefinition key_definition;
    CHECK(it->has_type());
    switch (it->type()) {
      case KeyData::KEY_TYPE_PASSWORD:
        key_definition.type = KeyDefinition::TYPE_PASSWORD;
        break;
      case KeyData::KEY_TYPE_CHALLENGE_RESPONSE:
        key_definition.type = KeyDefinition::TYPE_CHALLENGE_RESPONSE;
        break;
      case KeyData::KEY_TYPE_FINGERPRINT:
        // KEY_TYPE_FINGERPRINT means the key is a request for fingerprint auth
        // and does not really carry any auth information. KEY_TYPE_FINGERPRINT
        // is not expected to be used in GetKeyData.
        NOTREACHED();
        break;
      case KeyData::KEY_TYPE_KIOSK:
        key_definition.type = KeyDefinition::TYPE_PUBLIC_MOUNT;
        break;
    }
    key_definition.label = it->label();
    key_definition.revision = it->revision();

    // Extract |privileges|.
    const KeyPrivileges& privileges = it->privileges();
    if (privileges.add())
      key_definition.privileges |= PRIV_ADD;
    if (privileges.remove())
      key_definition.privileges |= PRIV_REMOVE;
    if (privileges.update())
      key_definition.privileges |= PRIV_MIGRATE;

    // Extract |policy|.
    key_definition.policy.low_entropy_credential =
        it->policy().low_entropy_credential();
    key_definition.policy.auth_locked = it->policy().auth_locked();

    // Extract |provider_data|.
    for (RepeatedPtrField<KeyProviderData::Entry>::const_iterator
             provider_data_it = it->provider_data().entry().begin();
         provider_data_it != it->provider_data().entry().end();
         ++provider_data_it) {
      // Extract |name|.
      key_definition.provider_data.push_back(
          KeyDefinition::ProviderData(provider_data_it->name()));
      KeyDefinition::ProviderData& provider_data =
          key_definition.provider_data.back();

      int data_items = 0;

      // Extract |number|.
      if (provider_data_it->has_number()) {
        provider_data.number =
            std::make_unique<int64_t>(provider_data_it->number());
        ++data_items;
      }

      // Extract |bytes|.
      if (provider_data_it->has_bytes()) {
        provider_data.bytes =
            std::make_unique<std::string>(provider_data_it->bytes());
        ++data_items;
      }

      DCHECK_EQ(1, data_items);
    }

    key_definitions.push_back(std::move(key_definition));
  }
  return key_definitions;
}

AuthorizationRequest CreateAuthorizationRequest(const std::string& label,
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
  if (!key_def.label.empty())
    data->set_label(key_def.label);

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

  if (key_def.revision > 0)
    data->set_revision(key_def.revision);

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
