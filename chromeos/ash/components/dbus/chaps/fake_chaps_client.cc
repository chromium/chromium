// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/chaps/fake_chaps_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/constants/pkcs11_definitions.h"

namespace ash {

void FakeChapsClient::GetSlotList(bool token_present,
                                  ArrayOfUint64Callback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::vector<uint64_t>(),
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::GetMechanismList(uint64_t slot_id,
                                       ArrayOfUint64Callback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::vector<uint64_t>(),
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::OpenSession(uint64_t slot_id,
                                  uint64_t flags,
                                  Uint64Callback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0,
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::CloseSession(uint64_t session_id,
                                   ResultCodeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::CreateObject(uint64_t session_id,
                                   const std::vector<uint8_t>& attributes,
                                   Uint64Callback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0,
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::DestroyObject(uint64_t session_id,
                                    uint64_t object_handle,
                                    ResultCodeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::GetAttributeValue(
    uint64_t session_id,
    uint64_t object_handle,
    const std::vector<uint8_t>& attributes_query,
    GetAttributeValueCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::vector<uint8_t>(),
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::SetAttributeValue(uint64_t session_id,
                                        uint64_t object_handle,
                                        const std::vector<uint8_t>& attributes,
                                        ResultCodeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::FindObjectsInit(uint64_t session_id,
                                      const std::vector<uint8_t>& attributes,
                                      ResultCodeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::FindObjects(uint64_t session_id,
                                  uint64_t max_object_count,
                                  ArrayOfUint64Callback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::vector<uint64_t>(),
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::FindObjectsFinal(uint64_t session_id,
                                       ResultCodeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::EncryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle,
    ResultCodeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::Encrypt(uint64_t session_id,
                              const std::vector<uint8_t>& data,
                              uint64_t max_out_length,
                              DataCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0, std::vector<uint8_t>(),
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::DecryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle,
    ResultCodeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::Decrypt(uint64_t session_id,
                              const std::vector<uint8_t>& data,
                              uint64_t max_out_length,
                              DataCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0, std::vector<uint8_t>(),
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::SignInit(uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle,
                               ResultCodeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::Sign(uint64_t session_id,
                           const std::vector<uint8_t>& data,
                           uint64_t max_out_length,
                           DataCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0, std::vector<uint8_t>(),
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::GenerateKeyPair(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    const std::vector<uint8_t>& public_attributes,
    const std::vector<uint8_t>& private_attributes,
    GenerateKeyPairCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0, 0,
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::WrapKey(uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              uint64_t wrapping_key_handle,
                              uint64_t key_handle,
                              uint64_t max_out_length,
                              DataCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0, std::vector<uint8_t>(),
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::UnwrapKey(uint64_t session_id,
                                uint64_t mechanism_type,
                                const std::vector<uint8_t>& mechanism_parameter,
                                uint64_t wrapping_key_handle,
                                const std::vector<uint8_t>& wrapped_key,
                                const std::vector<uint8_t>& attributes,
                                Uint64Callback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0,
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::DeriveKey(uint64_t session_id,
                                uint64_t mechanism_type,
                                const std::vector<uint8_t>& mechanism_parameter,
                                uint64_t base_key_handle,
                                const std::vector<uint8_t>& attributes,
                                Uint64Callback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0,
                                chromeos::PKCS11_CKR_GENERAL_ERROR));
}

}  // namespace ash
