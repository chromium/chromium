// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/chaps/chaps_client.h"

#include <stdint.h>

#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/dbus/chaps/fake_chaps_client.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

namespace ash {
namespace {
// 5 minutes, since some security element operations can take a while. Should be
// used for computationally heavy operations.
const int kDbusLongTimeoutMillis = base::Minutes(5).InMilliseconds();

// ChromeOS always uses default credentials.
constexpr uint8_t kIsolateCredential[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 0};

ChapsClient* g_instance = nullptr;

// Extract an array of uint64_t values from a dbus message associated with the
// `reader`.
bool PopArrayOfUint64s(dbus::MessageReader& reader,
                       std::vector<uint64_t>& array) {
  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader)) {
    return false;
  }

  uint64_t value;
  while (array_reader.HasMoreData() && array_reader.PopUint64(&value)) {
    array.emplace_back(value);
  }
  return true;
}

class ChapsClientImpl : public ChapsClient {
 public:
  void Init(dbus::Bus* bus);

  // Implements ChapsClient.
  void GetSlotList(bool token_present, ArrayOfUint64Callback callback) override;
  void GetMechanismList(uint64_t slot_id,
                        ArrayOfUint64Callback callback) override;
  void OpenSession(uint64_t slot_id,
                   uint64_t flags,
                   Uint64Callback callback) override;
  void CloseSession(uint64_t session_id, ResultCodeCallback callback) override;
  void CreateObject(uint64_t session_id,
                    const std::vector<uint8_t>& attributes,
                    Uint64Callback callback) override;
  void DestroyObject(uint64_t session_id,
                     uint64_t object_handle,
                     ResultCodeCallback callback) override;
  void GetAttributeValue(uint64_t session_id,
                         uint64_t object_handle,
                         const std::vector<uint8_t>& attributes_query,
                         GetAttributeValueCallback callback) override;
  void SetAttributeValue(uint64_t session_id,
                         uint64_t object_handle,
                         const std::vector<uint8_t>& attributes,
                         ResultCodeCallback callback) override;
  void FindObjectsInit(uint64_t session_id,
                       const std::vector<uint8_t>& attributes,
                       ResultCodeCallback callback) override;
  void FindObjects(uint64_t session_id,
                   uint64_t max_object_count,
                   ArrayOfUint64Callback callback) override;
  void FindObjectsFinal(uint64_t session_id,
                        ResultCodeCallback callback) override;
  void EncryptInit(uint64_t session_id,
                   uint64_t mechanism_type,
                   const std::vector<uint8_t>& mechanism_parameter,
                   uint64_t key_handle,
                   ResultCodeCallback callback) override;
  void Encrypt(uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               DataCallback callback) override;
  void DecryptInit(uint64_t session_id,
                   uint64_t mechanism_type,
                   const std::vector<uint8_t>& mechanism_parameter,
                   uint64_t key_handle,
                   ResultCodeCallback callback) override;
  void Decrypt(uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               DataCallback callback) override;
  void SignInit(uint64_t session_id,
                uint64_t mechanism_type,
                const std::vector<uint8_t>& mechanism_parameter,
                uint64_t key_handle,
                ResultCodeCallback callback) override;
  void Sign(uint64_t session_id,
            const std::vector<uint8_t>& data,
            uint64_t max_out_length,
            DataCallback callback) override;
  void GenerateKeyPair(uint64_t session_id,
                       uint64_t mechanism_type,
                       const std::vector<uint8_t>& mechanism_parameter,
                       const std::vector<uint8_t>& public_attributes,
                       const std::vector<uint8_t>& private_attributes,
                       GenerateKeyPairCallback callback) override;
  void WrapKey(uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t wrapping_key_handle,
               uint64_t key_handle,
               uint64_t max_out_length,
               DataCallback callback) override;
  void UnwrapKey(uint64_t session_id,
                 uint64_t mechanism_type,
                 const std::vector<uint8_t>& mechanism_parameter,
                 uint64_t wrapping_key_handle,
                 const std::vector<uint8_t>& wrapped_key,
                 const std::vector<uint8_t>& attributes,
                 Uint64Callback callback) override;
  void DeriveKey(uint64_t session_id,
                 uint64_t mechanism_type,
                 const std::vector<uint8_t>& mechanism_parameter,
                 uint64_t base_key_handle,
                 const std::vector<uint8_t>& attributes,
                 Uint64Callback callback) override;

 private:
  // Handles a response that contains a single result code.
  void ReceiveResultCode(ResultCodeCallback callback, dbus::Response* response);
  // Handles a response that contains a single uint64 (such as an object handle
  // or a session handle) and a result code.
  void ReceiveUint64(Uint64Callback callback, dbus::Response* response);
  // Handles a response that contains an array of uint64 (such as object
  // handles) and a result code.
  void ReceiveArrayOfUint64(ArrayOfUint64Callback callback,
                            dbus::Response* response);
  // Handles a response that contains a uint64 (actual_out_length), an array of
  // bytes (data) and a result code.
  void ReceiveData(DataCallback callback, dbus::Response* response);

  void DidGetAttributeValue(GetAttributeValueCallback callback,
                            dbus::Response* response);
  void DidGenerateKeyPair(GenerateKeyPairCallback callback,
                          dbus::Response* response);

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<dbus::ObjectProxy, LeakedDanglingUntriaged> proxy_ = nullptr;
  base::WeakPtrFactory<ChapsClientImpl> weak_factory_{this};
};

void ChapsClientImpl::Init(dbus::Bus* bus) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_ = bus->GetObjectProxy(chaps::kChapsServiceName,
                               dbus::ObjectPath(chaps::kChapsServicePath));
  DCHECK(proxy_);
}

void ChapsClientImpl::ReceiveResultCode(ResultCodeCallback callback,
                                        dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response) {
    return std::move(callback).Run(chaps::CKR_DBUS_EMPTY_RESPONSE_ERROR);
  }

  dbus::MessageReader reader(response);
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;
  if (!reader.PopUint32(&result_code)) {
    return std::move(callback).Run(chaps::CKR_DBUS_DECODING_ERROR);
  }

  return std::move(callback).Run(result_code);
}

void ChapsClientImpl::ReceiveUint64(Uint64Callback callback,
                                    dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response) {
    return std::move(callback).Run(chromeos::PKCS11_INVALID_SESSION_ID,
                                   chaps::CKR_DBUS_EMPTY_RESPONSE_ERROR);
  }

  dbus::MessageReader reader(response);

  uint64_t session_id = 0;
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;
  // Pop order matters.
  if (!reader.PopUint64(&session_id) || !reader.PopUint32(&result_code)) {
    return std::move(callback).Run(chromeos::PKCS11_INVALID_SESSION_ID,
                                   chaps::CKR_DBUS_DECODING_ERROR);
  }

  return std::move(callback).Run(session_id, result_code);
}

void ChapsClientImpl::ReceiveArrayOfUint64(ArrayOfUint64Callback callback,
                                           dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_EMPTY_RESPONSE_ERROR);
  }

  dbus::MessageReader reader(response);

  std::vector<uint64_t> slot_list;
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;
  // Pop order matters.
  if (!PopArrayOfUint64s(reader, slot_list) ||
      !reader.PopUint32(&result_code)) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_DECODING_ERROR);
  }

  return std::move(callback).Run(std::move(slot_list), result_code);
}

void ChapsClientImpl::ReceiveData(DataCallback callback,
                                  dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response) {
    return std::move(callback).Run(0, {}, chaps::CKR_DBUS_EMPTY_RESPONSE_ERROR);
  }

  dbus::MessageReader reader(response);
  uint64_t actual_out_length = 0;
  const uint8_t* data_bytes = nullptr;
  size_t data_length = 0;
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;
  // Pop order matters.
  if (!reader.PopUint64(&actual_out_length) ||
      !reader.PopArrayOfBytes(&data_bytes, &data_length) ||
      !reader.PopUint32(&result_code)) {
    return std::move(callback).Run(0, {}, chaps::CKR_DBUS_DECODING_ERROR);
  };

  return std::move(callback).Run(
      actual_out_length,
      std::vector<uint8_t>(data_bytes, data_bytes + data_length), result_code);
}

void ChapsClientImpl::GetSlotList(bool token_present,
                                  ArrayOfUint64Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kGetSlotListMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendBool(token_present);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveArrayOfUint64,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::GetMechanismList(uint64_t slot_id,
                                       ArrayOfUint64Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kGetMechanismListMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(slot_id);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveArrayOfUint64,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::OpenSession(uint64_t slot_id,
                                  uint64_t flags,
                                  Uint64Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kOpenSessionMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(slot_id);
  writer.AppendUint64(flags);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveUint64,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::CloseSession(uint64_t session_id,
                                   ResultCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kCloseSessionMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveResultCode,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::CreateObject(uint64_t session_id,
                                   const std::vector<uint8_t>& attributes,
                                   Uint64Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kCreateObjectMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendArrayOfBytes(attributes);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveUint64,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::DestroyObject(uint64_t session_id,
                                    uint64_t object_handle,
                                    ResultCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kDestroyObjectMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(object_handle);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveResultCode,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::GetAttributeValue(
    uint64_t session_id,
    uint64_t object_handle,
    const std::vector<uint8_t>& attributes_query,
    GetAttributeValueCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kGetAttributeValueMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(object_handle);
  writer.AppendArrayOfBytes(attributes_query);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::DidGetAttributeValue,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::DidGetAttributeValue(GetAttributeValueCallback callback,
                                           dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_EMPTY_RESPONSE_ERROR);
  }

  dbus::MessageReader reader(response);

  const uint8_t* attributes_bytes = nullptr;
  size_t attributes_length = 0;
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;
  // Pop order matters.
  if (!reader.PopArrayOfBytes(&attributes_bytes, &attributes_length) ||
      !reader.PopUint32(&result_code)) {
    return std::move(callback).Run({}, chaps::CKR_DBUS_DECODING_ERROR);
  }

  return std::move(callback).Run(
      std::vector<uint8_t>(attributes_bytes,
                           attributes_bytes + attributes_length),
      result_code);
}

void ChapsClientImpl::SetAttributeValue(uint64_t session_id,
                                        uint64_t object_handle,
                                        const std::vector<uint8_t>& attributes,
                                        ResultCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kSetAttributeValueMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(object_handle);
  writer.AppendArrayOfBytes(attributes);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveResultCode,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::FindObjectsInit(uint64_t session_id,
                                      const std::vector<uint8_t>& attributes,
                                      ResultCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kFindObjectsInitMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendArrayOfBytes(attributes);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveResultCode,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::FindObjects(uint64_t session_id,
                                  uint64_t max_object_count,
                                  ArrayOfUint64Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kFindObjectsMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(max_object_count);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveArrayOfUint64,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::FindObjectsFinal(uint64_t session_id,
                                       ResultCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kFindObjectsFinalMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);

  proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ChapsClientImpl::ReceiveResultCode,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::EncryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle,
    ResultCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kEncryptInitMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(mechanism_type);
  writer.AppendArrayOfBytes(mechanism_parameter);
  writer.AppendUint64(key_handle);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveResultCode,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::Encrypt(uint64_t session_id,
                              const std::vector<uint8_t>& data,
                              uint64_t max_out_length,
                              DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface, chaps::kEncryptMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendArrayOfBytes(data);
  writer.AppendUint64(max_out_length);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveData, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ChapsClientImpl::DecryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle,
    ResultCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kDecryptInitMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(mechanism_type);
  writer.AppendArrayOfBytes(mechanism_parameter);
  writer.AppendUint64(key_handle);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveResultCode,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::Decrypt(uint64_t session_id,
                              const std::vector<uint8_t>& data,
                              uint64_t max_out_length,
                              DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface, chaps::kDecryptMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendArrayOfBytes(data);
  writer.AppendUint64(max_out_length);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveData, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ChapsClientImpl::SignInit(uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle,
                               ResultCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface, chaps::kSignInitMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(mechanism_type);
  writer.AppendArrayOfBytes(mechanism_parameter);
  writer.AppendUint64(key_handle);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveResultCode,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::Sign(uint64_t session_id,
                           const std::vector<uint8_t>& data,
                           uint64_t max_out_length,
                           DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface, chaps::kSignMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendArrayOfBytes(data);
  writer.AppendUint64(max_out_length);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveData, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ChapsClientImpl::GenerateKeyPair(
    uint64_t session_id,
    uint64_t mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    const std::vector<uint8_t>& public_attributes,
    const std::vector<uint8_t>& private_attributes,
    GenerateKeyPairCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface,
                               chaps::kGenerateKeyPairMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(mechanism_type);
  writer.AppendArrayOfBytes(mechanism_parameter);
  writer.AppendArrayOfBytes(public_attributes);
  writer.AppendArrayOfBytes(private_attributes);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::DidGenerateKeyPair,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::DidGenerateKeyPair(GenerateKeyPairCallback callback,
                                         dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response) {
    return std::move(callback).Run(uint64_t(0), uint64_t(0),
                                   chaps::CKR_DBUS_EMPTY_RESPONSE_ERROR);
  }

  dbus::MessageReader reader(response);

  uint64_t public_key_id = 0;
  uint64_t private_key_id = 0;
  uint32_t result_code = chromeos::PKCS11_CKR_GENERAL_ERROR;
  // Pop order matters.
  if (!reader.PopUint64(&public_key_id) || !reader.PopUint64(&private_key_id) ||
      !reader.PopUint32(&result_code)) {
    return std::move(callback).Run(uint64_t(0), uint64_t(0),
                                   chaps::CKR_DBUS_DECODING_ERROR);
  }

  return std::move(callback).Run(uint64_t(public_key_id),
                                 uint64_t(private_key_id), result_code);
}

void ChapsClientImpl::WrapKey(uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              uint64_t wrapping_key_handle,
                              uint64_t key_handle,
                              uint64_t max_out_length,
                              DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface, chaps::kWrapKeyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(mechanism_type);
  writer.AppendArrayOfBytes(mechanism_parameter);
  writer.AppendUint64(wrapping_key_handle);
  writer.AppendUint64(key_handle);
  writer.AppendUint64(max_out_length);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveData, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ChapsClientImpl::UnwrapKey(uint64_t session_id,
                                uint64_t mechanism_type,
                                const std::vector<uint8_t>& mechanism_parameter,
                                uint64_t wrapping_key_handle,
                                const std::vector<uint8_t>& wrapped_key,
                                const std::vector<uint8_t>& attributes,
                                Uint64Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface, chaps::kUnwrapKeyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(mechanism_type);
  writer.AppendArrayOfBytes(mechanism_parameter);
  writer.AppendUint64(wrapping_key_handle);
  writer.AppendArrayOfBytes(wrapped_key);
  writer.AppendArrayOfBytes(attributes);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveUint64,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChapsClientImpl::DeriveKey(uint64_t session_id,
                                uint64_t mechanism_type,
                                const std::vector<uint8_t>& mechanism_parameter,
                                uint64_t base_key_handle,
                                const std::vector<uint8_t>& attributes,
                                Uint64Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dbus::MethodCall method_call(chaps::kChapsInterface, chaps::kDeriveKeyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(kIsolateCredential);
  writer.AppendUint64(session_id);
  writer.AppendUint64(mechanism_type);
  writer.AppendArrayOfBytes(mechanism_parameter);
  writer.AppendUint64(base_key_handle);
  writer.AppendArrayOfBytes(attributes);

  proxy_->CallMethod(
      &method_call, kDbusLongTimeoutMillis,
      base::BindOnce(&ChapsClientImpl::ReceiveUint64,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

}  // namespace

//   static
ChapsClient* ChapsClient::Get() {
  return g_instance;
}

ChapsClient::ChapsClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ChapsClient::~ChapsClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ChapsClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ChapsClientImpl())->Init(bus);
}

// static
void ChapsClient::InitializeFake() {
  new FakeChapsClient();
}

// static
void ChapsClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

}  // namespace ash
