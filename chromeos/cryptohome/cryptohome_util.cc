// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::ChallengeResponseKey;
using google::protobuf::RepeatedPtrField;

namespace cryptohome {

namespace {

bool IsEmpty(const base::Optional<BaseReply>& reply) {
  if (!reply.has_value()) {
    LOGIN_LOG(ERROR) << "Cryptohome call failed with empty reply.";
    return true;
  }
  return false;
}

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
  privileges->set_mount(key_def_privileges & PRIV_MOUNT);
  privileges->set_add(key_def_privileges & PRIV_ADD);
  privileges->set_remove(key_def_privileges & PRIV_REMOVE);
  privileges->set_update(key_def_privileges & PRIV_MIGRATE);
  privileges->set_authorized_update(key_def_privileges &
                                    PRIV_AUTHORIZED_UPDATE);
}

// TODO(crbug.com/797848): Add tests that cover this logic.
void KeyDefSecretToKeyAuthorizationSecret(
    const KeyDefinition::AuthorizationData::Secret& key_def_secret,
    KeyAuthorizationSecret* secret) {
  secret->mutable_usage()->set_encrypt(key_def_secret.encrypt);
  secret->mutable_usage()->set_sign(key_def_secret.sign);
  secret->set_wrapped(key_def_secret.wrapped);
  if (!key_def_secret.symmetric_key.empty())
    secret->set_symmetric_key(key_def_secret.symmetric_key);

  if (!key_def_secret.public_key.empty())
    secret->set_public_key(key_def_secret.public_key);
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

// TODO(crbug.com/797848): Add tests that cover this logic.
KeyAuthorizationData::KeyAuthorizationType GetKeyAuthDataType(
    KeyDefinition::AuthorizationData::Type key_def_auth_data_type) {
  switch (key_def_auth_data_type) {
    case KeyDefinition::AuthorizationData::TYPE_HMACSHA256:
      return KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_HMACSHA256;
    case KeyDefinition::AuthorizationData::TYPE_AES256CBC_HMACSHA256:
      return KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_AES256CBC_HMACSHA256;
  }
}

}  // namespace

MountError MountExReplyToMountError(const base::Optional<BaseReply>& reply) {
  if (IsEmpty(reply))
    return MOUNT_ERROR_FATAL;

  if (!reply->HasExtension(MountReply::reply)) {
    LOGIN_LOG(ERROR) << "MountEx failed with no MountReply extension in reply.";
    return MOUNT_ERROR_FATAL;
  }
  return CryptohomeErrorToMountError(reply->error());
}

MountError BaseReplyToMountError(const base::Optional<BaseReply>& reply) {
  if (IsEmpty(reply))
    return MOUNT_ERROR_FATAL;

  return CryptohomeErrorToMountError(reply->error());
}

MountError GetKeyDataReplyToMountError(const base::Optional<BaseReply>& reply) {
  if (IsEmpty(reply))
    return MOUNT_ERROR_FATAL;

  if (!reply->HasExtension(GetKeyDataReply::reply)) {
    LOGIN_LOG(ERROR)
        << "GetKeyDataEx failed with no GetKeyDataReply extension in reply.";
    return MOUNT_ERROR_FATAL;
  }
  return CryptohomeErrorToMountError(reply->error());
}

// TODO(crbug.com/797848): Finish testing this method.
std::vector<KeyDefinition> GetKeyDataReplyToKeyDefinitions(
    const base::Optional<BaseReply>& reply) {
  const RepeatedPtrField<KeyData>& key_data =
      reply->GetExtension(GetKeyDataReply::reply).key_data();
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
    }
    key_definition.label = it->label();
    key_definition.revision = it->revision();

    // Extract |privileges|.
    const KeyPrivileges& privileges = it->privileges();
    if (privileges.mount())
      key_definition.privileges |= PRIV_MOUNT;
    if (privileges.add())
      key_definition.privileges |= PRIV_ADD;
    if (privileges.remove())
      key_definition.privileges |= PRIV_REMOVE;
    if (privileges.update())
      key_definition.privileges |= PRIV_MIGRATE;
    if (privileges.authorized_update())
      key_definition.privileges |= PRIV_AUTHORIZED_UPDATE;

    // Extract |policy|.
    key_definition.policy.low_entropy_credential =
        it->policy().low_entropy_credential();
    key_definition.policy.auth_locked = it->policy().auth_locked();

    // Extract |authorization_data|.
    for (RepeatedPtrField<KeyAuthorizationData>::const_iterator auth_it =
             it->authorization_data().begin();
         auth_it != it->authorization_data().end(); ++auth_it) {
      key_definition.authorization_data.push_back(
          KeyDefinition::AuthorizationData());
      KeyAuthorizationDataToAuthorizationData(
          *auth_it, &key_definition.authorization_data.back());
    }

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
        provider_data.number.reset(new int64_t(provider_data_it->number()));
        ++data_items;
      }

      // Extract |bytes|.
      if (provider_data_it->has_bytes()) {
        provider_data.bytes.reset(new std::string(provider_data_it->bytes()));
        ++data_items;
      }

      DCHECK_EQ(1, data_items);
    }

    key_definitions.push_back(std::move(key_definition));
  }
  return key_definitions;
}

int64_t AccountDiskUsageReplyToUsageSize(
    const base::Optional<BaseReply>& reply) {
  if (IsEmpty(reply))
    return -1;

  if (reply->has_error() && reply->error() != CRYPTOHOME_ERROR_NOT_SET) {
    LOGIN_LOG(ERROR) << "GetAccountDiskUsage failed with error: "
                     << reply->error();
    return -1;
  }
  if (!reply->HasExtension(GetAccountDiskUsageReply::reply)) {
    LOGIN_LOG(ERROR) << "GetAccountDiskUsage failed with no "
                        "GetAccountDiskUsageReply extension in reply.";
    return -1;
  }
  return reply->GetExtension(GetAccountDiskUsageReply::reply).size();
}

const std::string& MountExReplyToMountHash(const BaseReply& reply) {
  return reply.GetExtension(MountReply::reply).sanitized_username();
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
  }

  if (key_def.revision > 0)
    data->set_revision(key_def.revision);

  if (key_def.privileges != 0) {
    KeyDefPrivilegesToKeyPrivileges(key_def.privileges,
                                    data->mutable_privileges());
  }

  for (const auto& key_def_auth_data : key_def.authorization_data) {
    KeyAuthorizationData* auth_data = data->add_authorization_data();
    auth_data->set_type(GetKeyAuthDataType(key_def_auth_data.type));
    for (const auto& key_def_secret : key_def_auth_data.secrets) {
      KeyDefSecretToKeyAuthorizationSecret(key_def_secret,
                                           auth_data->add_secrets());
    }
  }

  for (const auto& provider_data : key_def.provider_data) {
    KeyDefProviderDataToKeyProviderDataEntry(
        provider_data, data->mutable_provider_data()->add_entry());
  }
}

// TODO(crbug.com/797848): Finish testing this method.
MountError CryptohomeErrorToMountError(CryptohomeErrorCode code) {
  switch (code) {
    case CRYPTOHOME_ERROR_NOT_SET:
      return MOUNT_ERROR_NONE;
    case CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND:
      return MOUNT_ERROR_USER_DOES_NOT_EXIST;
    case CRYPTOHOME_ERROR_NOT_IMPLEMENTED:
    case CRYPTOHOME_ERROR_MOUNT_FATAL:
    case CRYPTOHOME_ERROR_KEY_QUOTA_EXCEEDED:
    case CRYPTOHOME_ERROR_BACKING_STORE_FAILURE:
    case CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_FINALIZE_FAILED:
    case CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_GET_FAILED:
    case CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_SET_FAILED:
    case CRYPTOHOME_ERROR_INVALID_ARGUMENT:
      return MOUNT_ERROR_FATAL;
    case CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND:
    case CRYPTOHOME_ERROR_KEY_NOT_FOUND:
    case CRYPTOHOME_ERROR_MIGRATE_KEY_FAILED:
    case CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED:
      return MOUNT_ERROR_KEY_FAILURE;
    case CRYPTOHOME_ERROR_TPM_COMM_ERROR:
      return MOUNT_ERROR_TPM_COMM_ERROR;
    case CRYPTOHOME_ERROR_TPM_DEFEND_LOCK:
      return MOUNT_ERROR_TPM_DEFEND_LOCK;
    case CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY:
      return MOUNT_ERROR_MOUNT_POINT_BUSY;
    case CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT:
      return MOUNT_ERROR_TPM_NEEDS_REBOOT;
    case CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED:
    case CRYPTOHOME_ERROR_KEY_LABEL_EXISTS:
    case CRYPTOHOME_ERROR_UPDATE_SIGNATURE_INVALID:
      return MOUNT_ERROR_KEY_FAILURE;
    case CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION:
      return MOUNT_ERROR_OLD_ENCRYPTION;
    case CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE:
      return MOUNT_ERROR_PREVIOUS_MIGRATION_INCOMPLETE;
    case CRYPTOHOME_ERROR_REMOVE_FAILED:
      return MOUNT_ERROR_REMOVE_FAILED;
    // TODO(crbug.com/797563): Split the error space and/or handle everything.
    case CRYPTOHOME_ERROR_LOCKBOX_SIGNATURE_INVALID:
    case CRYPTOHOME_ERROR_LOCKBOX_CANNOT_SIGN:
    case CRYPTOHOME_ERROR_BOOT_ATTRIBUTE_NOT_FOUND:
    case CRYPTOHOME_ERROR_BOOT_ATTRIBUTES_CANNOT_SIGN:
    case CRYPTOHOME_ERROR_TPM_EK_NOT_AVAILABLE:
    case CRYPTOHOME_ERROR_ATTESTATION_NOT_READY:
    case CRYPTOHOME_ERROR_CANNOT_CONNECT_TO_CA:
    case CRYPTOHOME_ERROR_CA_REFUSED_ENROLLMENT:
    case CRYPTOHOME_ERROR_CA_REFUSED_CERTIFICATE:
    case CRYPTOHOME_ERROR_INTERNAL_ATTESTATION_ERROR:
    case CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_INVALID:
    case CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_CANNOT_STORE:
    case CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_CANNOT_REMOVE:
    case CRYPTOHOME_ERROR_UPDATE_USER_ACTIVITY_TIMESTAMP_FAILED:
    case CRYPTOHOME_ERROR_FAILED_TO_EXTEND_PCR:
    case CRYPTOHOME_ERROR_FAILED_TO_READ_PCR:
    case CRYPTOHOME_ERROR_PCR_ALREADY_EXTENDED:
    case CRYPTOHOME_ERROR_TPM_UPDATE_REQUIRED:
      NOTREACHED();
      return MOUNT_ERROR_FATAL;
  }
}

void KeyAuthorizationDataToAuthorizationData(
    const KeyAuthorizationData& authorization_data_proto,
    KeyDefinition::AuthorizationData* authorization_data) {
  switch (authorization_data_proto.type()) {
    case KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_HMACSHA256:
      authorization_data->type =
          KeyDefinition::AuthorizationData::TYPE_HMACSHA256;
      break;
    case KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_AES256CBC_HMACSHA256:
      authorization_data->type =
          KeyDefinition::AuthorizationData::TYPE_AES256CBC_HMACSHA256;
      break;
  }

  for (const auto& secret : authorization_data_proto.secrets()) {
    authorization_data->secrets.push_back(
        KeyDefinition::AuthorizationData::Secret(
            secret.usage().encrypt(), secret.usage().sign(),
            secret.symmetric_key(), secret.public_key(), secret.wrapped()));
  }
}

}  // namespace cryptohome
