// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor.h"

#include <utility>

#include "base/functional/bind.h"

namespace ash {

namespace device_sync {

namespace {

const char kSinglePayloadId[] = "single_payload_id";

void ForwardResultToSingleInputCallback(
    CryptAuthEciesEncryptor::SingleInputCallback single_input_callback,
    const CryptAuthEciesEncryptor::IdToOutputMap& id_to_output_map) {
  const auto it = id_to_output_map.find(kSinglePayloadId);
  DCHECK(it != id_to_output_map.end());

  std::move(single_input_callback).Run(it->second);
}

}  // namespace

CryptAuthEciesEncryptor::PayloadAndKey::PayloadAndKey() = default;

CryptAuthEciesEncryptor::PayloadAndKey::PayloadAndKey(
    const std::string& payload,
    const std::string& key)
    : payload(payload), key(key) {}

bool CryptAuthEciesEncryptor::PayloadAndKey::operator==(
    const PayloadAndKey& other) const {
  return payload == other.payload && key == other.key;
}

CryptAuthEciesEncryptor::CryptAuthEciesEncryptor() = default;

CryptAuthEciesEncryptor::~CryptAuthEciesEncryptor() = default;

void CryptAuthEciesEncryptor::Encrypt(
    const std::string& unencrypted_payload,
    const std::string& encrypting_public_key,
    SingleInputCallback encryption_finished_callback) {
  BatchEncrypt({{kSinglePayloadId,
                 PayloadAndKey(unencrypted_payload, encrypting_public_key)}},
               base::BindOnce(&ForwardResultToSingleInputCallback,
                              std::move(encryption_finished_callback)));
}

void CryptAuthEciesEncryptor::Decrypt(
    const std::string& encrypted_payload,
    const std::string& decrypting_private_key,
    SingleInputCallback decryption_finished_callback) {
  BatchDecrypt({{kSinglePayloadId,
                 PayloadAndKey(encrypted_payload, decrypting_private_key)}},
               base::BindOnce(&ForwardResultToSingleInputCallback,
                              std::move(decryption_finished_callback)));
}

void CryptAuthEciesEncryptor::BatchEncrypt(
    const IdToInputMap& id_to_payload_and_key_map,
    BatchCallback encryption_finished_callback) {
  ProcessInput(id_to_payload_and_key_map,
               std::move(encryption_finished_callback));

  OnBatchEncryptionStarted();
}

void CryptAuthEciesEncryptor::BatchDecrypt(
    const IdToInputMap& id_to_payload_and_key_map,
    BatchCallback decryption_finished_callback) {
  ProcessInput(id_to_payload_and_key_map,
               std::move(decryption_finished_callback));

  OnBatchDecryptionStarted();
}

void CryptAuthEciesEncryptor::OnAttemptFinished(
    const IdToOutputMap& id_to_output_map) {
  std::move(callback_).Run(id_to_output_map);
}

void CryptAuthEciesEncryptor::ProcessInput(const IdToInputMap& id_to_input_map,
                                           BatchCallback callback) {
  DCHECK(!id_to_input_map.empty());
  DCHECK(callback);
  for (const auto& id_payload_and_key_pair : id_to_input_map)
    DCHECK(!id_payload_and_key_pair.second.key.empty());

  // Fail if a public method has already been called.
  DCHECK(id_to_input_map_.empty());

  id_to_input_map_ = id_to_input_map;
  callback_ = std::move(callback);
}

}  // namespace device_sync

}  // namespace ash
