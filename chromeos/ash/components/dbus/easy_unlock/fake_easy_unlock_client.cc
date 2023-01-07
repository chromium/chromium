// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/easy_unlock/fake_easy_unlock_client.h"

#include <memory>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/stringprintf.h"

namespace {

// Keys generated using |GenerateEcP256KeyPair| are in the following format:
// "{<key_type>: <key_pair_index>}".
// <key_pair_index> is an integer identifying
// the key pair.
// <key_type> specifies whether the key is public or private. It can have one
// of the following valies:
const char kEc256PrivateKeyKey[] = "ec_p256_private_key";
const char kEc256PublicKeyKey[] = "ec_p256_public_key";

// Extracts key pair index from a key in format "<key_type>: <key_pair_index>}".
int ExtractKeyPairIndexFromKey(const std::string& key,
                               const std::string& key_type) {
  JSONStringValueDeserializer deserializer(key);
  std::unique_ptr<base::Value> json_value =
      deserializer.Deserialize(NULL, NULL);
  if (!json_value)
    return -1;

  if (!json_value->is_dict())
    return -1;

  return json_value->GetDict().FindInt(key_type).value_or(-1);
}

}  // namespace

namespace ash {

// static
bool FakeEasyUnlockClient::IsEcP256KeyPair(const std::string& private_key,
                                           const std::string& public_key) {
  int private_key_index =
      ExtractKeyPairIndexFromKey(private_key, kEc256PrivateKeyKey);
  int public_key_index =
      ExtractKeyPairIndexFromKey(public_key, kEc256PublicKeyKey);

  return private_key_index > 0 && public_key_index == private_key_index;
}

FakeEasyUnlockClient::FakeEasyUnlockClient() : generated_keys_count_(0) {}

FakeEasyUnlockClient::~FakeEasyUnlockClient() = default;

void FakeEasyUnlockClient::Init(dbus::Bus* bus) {}

void FakeEasyUnlockClient::GenerateEcP256KeyPair(KeyPairCallback callback) {
  ++generated_keys_count_;

  std::move(callback).Run(
      base::StringPrintf("{\"%s\": %d}", kEc256PrivateKeyKey,
                         generated_keys_count_),
      base::StringPrintf("{\"%s\": %d}", kEc256PublicKeyKey,
                         generated_keys_count_));
}

void FakeEasyUnlockClient::WrapPublicKey(const std::string& key_algorithm,
                                         const std::string& public_key,
                                         DataCallback callback) {
  std::move(callback).Run(base::StringPrintf(
      "{\"wrapped_key\": {\"algorithm\":\"%s\", \"key\":\"%s\"}}",
      key_algorithm.c_str(), public_key.c_str()));
}

void FakeEasyUnlockClient::PerformECDHKeyAgreement(
    const std::string& private_key,
    const std::string& public_key,
    DataCallback callback) {
  int private_key_index =
      ExtractKeyPairIndexFromKey(private_key, kEc256PrivateKeyKey);
  int public_key_index =
      ExtractKeyPairIndexFromKey(public_key, kEc256PublicKeyKey);
  if (private_key_index < 0 || public_key_index < 0) {
    std::move(callback).Run(std::string());
    return;
  }

  // ECDH key agreement should be commutative in respect to key pairs to which
  // used keys belong, i.e. (key_pair[1].private_key, key_pair[2].public_key)
  // and (key_pair[2].private_key, key_pair[1].public_key) should produce the
  // same shared key. To achieve this, identify the created key by sum and
  // product of the used key pairs.
  std::move(callback).Run(base::StringPrintf(
      "{\"secret_key\": [%d, %d]}", private_key_index + public_key_index,
      private_key_index * public_key_index));
}

void FakeEasyUnlockClient::CreateSecureMessage(
    const std::string& payload,
    const CreateSecureMessageOptions& options,
    DataCallback callback) {
  std::move(callback).Run(base::StringPrintf(
      "{\"securemessage\": {"
      "\"payload\": \"%s\","
      "\"key\": \"%s\","
      "\"associated_data\": \"%s\","
      "\"public_metadata\": \"%s\","
      "\"verification_key_id\": \"%s\","
      "\"decryption_key_id\": \"%s\","
      "\"encryption_type\": \"%s\","
      "\"signature_type\": \"%s\""
      "}}",
      payload.c_str(), options.key.c_str(), options.associated_data.c_str(),
      options.public_metadata.c_str(), options.verification_key_id.c_str(),
      options.decryption_key_id.c_str(), options.encryption_type.c_str(),
      options.signature_type.c_str()));
}

void FakeEasyUnlockClient::UnwrapSecureMessage(
    const std::string& message,
    const UnwrapSecureMessageOptions& options,
    DataCallback callback) {
  // TODO(tbarzic): Verify that |message| is in the format returned by
  // |CreateSecureMessage| and extract payload, metadata and
  // verification_key_id from there.
  std::move(callback).Run(base::StringPrintf(
      "{\"unwrapped_securemessage\": {"
      "\"message\": \"%s\","
      "\"key\": \"%s\","
      "\"associated_data\": \"%s\","
      "\"encryption_type\": \"%s\","
      "\"signature_type\": \"%s\""
      "}}",
      message.c_str(), options.key.c_str(), options.associated_data.c_str(),
      options.encryption_type.c_str(), options.signature_type.c_str()));
}

}  // namespace ash
