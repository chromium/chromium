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
#include "base/types/expected.h"
#include "chromeos/constants/pkcs11_definitions.h"

namespace ash {
namespace {
void Post(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

// Finds the value for the `key` in `map` or returns `default_result` if the
// `key` doesn't exist.
template <typename K, typename V>
V FindOr(base::flat_map<K, V>& map, K key, V default_result) {
  auto iter = map.find(key);
  if (iter == map.end()) {
    return default_result;
  }
  return iter->second;
}

}  // namespace

FakeChapsClient::FakeChapsClient() = default;
FakeChapsClient::~FakeChapsClient() = default;

// Currently this method always fails. This can be implemented more
// realistically when needed.
void FakeChapsClient::GetSlotList(bool token_present,
                                  ArrayOfUint64Callback callback) {
  return Post(base::BindOnce(std::move(callback), std::vector<uint64_t>(),
                             chromeos::PKCS11_CKR_GENERAL_ERROR));
}

// Currently this method always fails. This can be implemented more
// realistically when needed.
void FakeChapsClient::GetMechanismList(uint64_t slot_id,
                                       ArrayOfUint64Callback callback) {
  return Post(base::BindOnce(std::move(callback), std::vector<uint64_t>(),
                             chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::OpenSession(uint64_t slot_id,
                                  uint64_t flags,
                                  Uint64Callback callback) {
  ++open_session_call_counter_;

  if (simulated_open_session_result_code_.has_value()) {
    return Post(base::BindOnce(std::move(callback), 0,
                               simulated_open_session_result_code_.value()));
  }

  uint64_t new_session_id = next_session_id_++;
  open_sessions_[new_session_id] = slot_id;

  return Post(base::BindOnce(std::move(callback), new_session_id,
                             chromeos::PKCS11_CKR_OK));
}

void FakeChapsClient::CloseSession(uint64_t session_id,
                                   ResultCodeCallback callback) {
  auto session_iter = open_sessions_.find(session_id);
  if (session_iter == open_sessions_.end()) {
    return Post(base::BindOnce(std::move(callback),
                               chromeos::PKCS11_CKR_SESSION_CLOSED));
  }

  open_sessions_.erase(session_iter);
  return Post(base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_OK));
}

// Currently this method just saves the `attributes` blob that is passed. Real
// Chaps converts the attributes in some cases. This can be implemented more
// realistically when needed.
void FakeChapsClient::CreateObject(uint64_t session_id,
                                   const std::vector<uint8_t>& attributes,
                                   Uint64Callback callback) {
  auto session_iter = open_sessions_.find(session_id);
  if (session_iter == open_sessions_.end()) {
    return Post(base::BindOnce(std::move(callback), 0,
                               chromeos::PKCS11_CKR_SESSION_CLOSED));
  }
  uint64_t slot_id = session_iter->second;

  uint64_t new_object_id = next_object_id_++;
  stored_objects_[slot_id][new_object_id] = attributes;

  return Post(base::BindOnce(std::move(callback), new_object_id,
                             chromeos::PKCS11_CKR_OK));
}

void FakeChapsClient::DestroyObject(uint64_t session_id,
                                    uint64_t object_handle,
                                    ResultCodeCallback callback) {
  base::expected<ObjectLocation, uint32_t> location =
      FindObject(session_id, object_handle);
  if (!location.has_value()) {
    return Post(base::BindOnce(std::move(callback), location.error()));
  }

  stored_objects_[location->slot_id].erase(location->iterator);
  return Post(base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_OK));
}

// Currently it just returns all the attributes that are stored for the
// `object_handle`. Real Chaps parses `attributes_query` and only returns
// attributes provided in it. This can be implemented more realistically when
// needed.
void FakeChapsClient::GetAttributeValue(
    uint64_t session_id,
    uint64_t object_handle,
    const std::vector<uint8_t>& attributes_query,
    GetAttributeValueCallback callback) {
  base::expected<ObjectLocation, uint32_t> location =
      FindObject(session_id, object_handle);
  if (!location.has_value()) {
    return Post(base::BindOnce(std::move(callback), std::vector<uint8_t>(),
                               location.error()));
  }

  const std::vector<uint8_t>& attrs = location->iterator->second;
  return Post(
      base::BindOnce(std::move(callback), attrs, chromeos::PKCS11_CKR_OK));
}

void FakeChapsClient::SetAttributeValue(uint64_t session_id,
                                        uint64_t object_handle,
                                        const std::vector<uint8_t>& attributes,
                                        ResultCodeCallback callback) {
  base::expected<ObjectLocation, uint32_t> location =
      FindObject(session_id, object_handle);
  if (!location.has_value()) {
    return Post(base::BindOnce(std::move(callback), location.error()));
  }

  location->iterator->second = attributes;
  return Post(base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_OK));
}

void FakeChapsClient::FindObjectsInit(uint64_t session_id,
                                      const std::vector<uint8_t>& attributes,
                                      ResultCodeCallback callback) {
  auto session_iter = open_sessions_.find(session_id);
  if (session_iter == open_sessions_.end()) {
    return Post(base::BindOnce(std::move(callback),
                               chromeos::PKCS11_CKR_SESSION_CLOSED));
  }

  if (FindOr(find_objects_init_called_, session_id, false)) {
    // Another "FindObject" sequence is already in progress, it's not expected
    // to have more than one in parallel for the same `session_id`.
    return Post(base::BindOnce(std::move(callback),
                               chromeos::PKCS11_CKR_GENERAL_ERROR));
  }
  find_objects_init_called_[session_id] = true;

  return Post(base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_OK));
}

// Currently this method just returns all the objects for the `session_id`. Real
// Chaps filters the objects based on the `attributes` from the
// `FindObjectsInit` call. This can be implemented more realistically when
// needed.
void FakeChapsClient::FindObjects(uint64_t session_id,
                                  uint64_t max_object_count,
                                  ArrayOfUint64Callback callback) {
  if (!FindOr(find_objects_init_called_, session_id, false)) {
    // `FindObjectsInit` was not called.
    return Post(base::BindOnce(std::move(callback), std::vector<uint64_t>(),
                               chromeos::PKCS11_CKR_GENERAL_ERROR));
  }

  auto session_iter = open_sessions_.find(session_id);
  if (session_iter == open_sessions_.end()) {
    return Post(base::BindOnce(std::move(callback), std::vector<uint64_t>(),
                               chromeos::PKCS11_CKR_SESSION_CLOSED));
  }
  uint64_t slot_id = session_iter->second;

  std::vector<uint64_t> result;
  for (const auto& [k, v] : stored_objects_[slot_id]) {
    result.push_back(k);
  }

  return Post(base::BindOnce(std::move(callback), std::move(result),
                             chromeos::PKCS11_CKR_OK));
}

void FakeChapsClient::FindObjectsFinal(uint64_t session_id,
                                       ResultCodeCallback callback) {
  if (!FindOr(find_objects_init_called_, session_id, false)) {
    // `FindObjectsInit` was not called.
    return Post(base::BindOnce(std::move(callback),
                               chromeos::PKCS11_CKR_GENERAL_ERROR));
  }
  find_objects_init_called_[session_id] = false;

  return Post(base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_OK));
}

void FakeChapsClient::EncryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle,
    ResultCodeCallback callback) {
  return Post(
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::Encrypt(uint64_t session_id,
                              const std::vector<uint8_t>& data,
                              uint64_t max_out_length,
                              DataCallback callback) {
  return Post(base::BindOnce(std::move(callback), 0, std::vector<uint8_t>(),
                             chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::DecryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle,
    ResultCodeCallback callback) {
  return Post(
      base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_GENERAL_ERROR));
}

void FakeChapsClient::Decrypt(uint64_t session_id,
                              const std::vector<uint8_t>& data,
                              uint64_t max_out_length,
                              DataCallback callback) {
  return Post(base::BindOnce(std::move(callback), 0, std::vector<uint8_t>(),
                             chromeos::PKCS11_CKR_GENERAL_ERROR));
}

// Ideally this method should check that the arguments make sense. This can be
// implemented more realistically when needed.
void FakeChapsClient::SignInit(uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle,
                               ResultCodeCallback callback) {
  base::expected<ObjectLocation, uint32_t> key_location =
      FindObject(session_id, key_handle);
  if (!key_location.has_value()) {
    return Post(base::BindOnce(std::move(callback), key_location.error()));
  }

  if (FindOr(sign_init_called_, session_id, false)) {
    // Another "Sign" sequence is already in progress.
    return Post(base::BindOnce(std::move(callback),
                               chromeos::PKCS11_CKR_GENERAL_ERROR));
  }
  sign_init_called_[session_id] = true;

  return Post(base::BindOnce(std::move(callback), chromeos::PKCS11_CKR_OK));
}

// Currently it just returns `data` back as the signature, this is not
// realistic. This can be implemented more realistically when needed.
void FakeChapsClient::Sign(uint64_t session_id,
                           const std::vector<uint8_t>& data,
                           uint64_t max_out_length,
                           DataCallback callback) {
  if (!FindOr(sign_init_called_, session_id, false)) {
    // `SignInit` was not called.
    return Post(base::BindOnce(std::move(callback), 0, std::vector<uint8_t>(),
                               chromeos::PKCS11_CKR_GENERAL_ERROR));
  }

  return Post(base::BindOnce(std::move(callback), data.size(), data,
                             chromeos::PKCS11_CKR_OK));
}

// Currently it just creates a pair of object ids and saving the attributes for
// them. Read Chaps would convert some of the attributes. This can be
// implemented more realistically when needed.
void FakeChapsClient::GenerateKeyPair(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    const std::vector<uint8_t>& public_attributes,
    const std::vector<uint8_t>& private_attributes,
    GenerateKeyPairCallback callback) {
  auto session_iter = open_sessions_.find(session_id);
  if (session_iter == open_sessions_.end()) {
    return Post(base::BindOnce(std::move(callback), 0, 0,
                               chromeos::PKCS11_CKR_SESSION_CLOSED));
  }
  uint64_t slot_id = session_iter->second;

  uint64_t pub_key_id = next_object_id_++;
  stored_objects_[slot_id][pub_key_id] = public_attributes;

  uint64_t priv_key_id = next_object_id_++;
  stored_objects_[slot_id][priv_key_id] = private_attributes;

  return Post(base::BindOnce(std::move(callback), pub_key_id, priv_key_id,
                             chromeos::PKCS11_CKR_OK));
}

// Currently this method always fails. This can be implemented more
// realistically when needed.
void FakeChapsClient::WrapKey(uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              uint64_t wrapping_key_handle,
                              uint64_t key_handle,
                              uint64_t max_out_length,
                              DataCallback callback) {
  return Post(base::BindOnce(std::move(callback), 0, std::vector<uint8_t>(),
                             chromeos::PKCS11_CKR_GENERAL_ERROR));
}

// Currently this method always fails. This can be implemented more
// realistically when needed.
void FakeChapsClient::UnwrapKey(uint64_t session_id,
                                uint64_t mechanism_type,
                                const std::vector<uint8_t>& mechanism_parameter,
                                uint64_t wrapping_key_handle,
                                const std::vector<uint8_t>& wrapped_key,
                                const std::vector<uint8_t>& attributes,
                                Uint64Callback callback) {
  return Post(base::BindOnce(std::move(callback), 0,
                             chromeos::PKCS11_CKR_GENERAL_ERROR));
}

// Currently this method always fails. This can be implemented more
// realistically when needed.
void FakeChapsClient::DeriveKey(uint64_t session_id,
                                uint64_t mechanism_type,
                                const std::vector<uint8_t>& mechanism_parameter,
                                uint64_t base_key_handle,
                                const std::vector<uint8_t>& attributes,
                                Uint64Callback callback) {
  return Post(base::BindOnce(std::move(callback), 0,
                             chromeos::PKCS11_CKR_GENERAL_ERROR));
}

base::expected<FakeChapsClient::ObjectLocation, uint32_t>
FakeChapsClient::FindObject(uint64_t session_id, uint64_t object_handle) {
  auto session_iter = open_sessions_.find(session_id);
  if (session_iter == open_sessions_.end()) {
    return base::unexpected(chromeos::PKCS11_CKR_SESSION_CLOSED);
  }
  uint64_t slot_id = session_iter->second;

  auto object_iter = stored_objects_[slot_id].find(object_handle);
  if (object_iter == stored_objects_[slot_id].end()) {
    return base::unexpected(chromeos::PKCS11_CKR_GENERAL_ERROR);
  }

  return ObjectLocation{slot_id, object_iter};
}

void FakeChapsClient::SimulateOpenSessionError(
    std::optional<uint32_t> result_code) {
  simulated_open_session_result_code_ = std::move(result_code);
}

int FakeChapsClient::GetAndResetOpenSessionCounter() {
  return std::exchange(open_session_call_counter_, 0);
}

}  // namespace ash
