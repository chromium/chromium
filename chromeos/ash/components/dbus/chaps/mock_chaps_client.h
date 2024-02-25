// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_MOCK_CHAPS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_MOCK_CHAPS_CLIENT_H_

#include "chromeos/ash/components/dbus/chaps/chaps_client.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockChapsClient : public ChapsClient {
 public:
  MockChapsClient();
  ~MockChapsClient() override;

  MOCK_METHOD(void,
              GetSlotList,
              (bool token_present, ArrayOfUint64Callback callback),
              (override));
  MOCK_METHOD(void,
              GetMechanismList,
              (uint64_t slot_id, ArrayOfUint64Callback callback),
              (override));
  MOCK_METHOD(void,
              OpenSession,
              (uint64_t slot_id, uint64_t flags, Uint64Callback callback),
              (override));
  MOCK_METHOD(void,
              CloseSession,
              (uint64_t session_id, ResultCodeCallback callback),
              (override));
  MOCK_METHOD(void,
              CreateObject,
              (uint64_t session_id,
               const std::vector<uint8_t>& attributes,
               Uint64Callback callback),
              (override));
  MOCK_METHOD(void,
              DestroyObject,
              (uint64_t session_id,
               uint64_t object_handle,
               ResultCodeCallback callback),
              (override));
  MOCK_METHOD(void,
              GetAttributeValue,
              (uint64_t session_id,
               uint64_t object_handle,
               const std::vector<uint8_t>& attributes_query,
               GetAttributeValueCallback callback),
              (override));
  MOCK_METHOD(void,
              SetAttributeValue,
              (uint64_t session_id,
               uint64_t object_handle,
               const std::vector<uint8_t>& attributes,
               ResultCodeCallback callback),
              (override));
  MOCK_METHOD(void,
              FindObjectsInit,
              (uint64_t session_id,
               const std::vector<uint8_t>& attributes,
               ResultCodeCallback callback),
              (override));
  MOCK_METHOD(void,
              FindObjects,
              (uint64_t session_id,
               uint64_t max_object_count,
               ArrayOfUint64Callback callback),
              (override));
  MOCK_METHOD(void,
              FindObjectsFinal,
              (uint64_t session_id, ResultCodeCallback callback),
              (override));
  MOCK_METHOD(void,
              EncryptInit,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t key_handle,
               ResultCodeCallback callback),
              (override));
  MOCK_METHOD(void,
              Encrypt,
              (uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               DataCallback callback),
              (override));
  MOCK_METHOD(void,
              DecryptInit,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t key_handle,
               ResultCodeCallback callback),
              (override));
  MOCK_METHOD(void,
              Decrypt,
              (uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               DataCallback callback),
              (override));
  MOCK_METHOD(void,
              SignInit,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t key_handle,
               ResultCodeCallback callback),
              (override));
  MOCK_METHOD(void,
              Sign,
              (uint64_t session_id,
               const std::vector<uint8_t>& data,
               uint64_t max_out_length,
               DataCallback callback),
              (override));
  MOCK_METHOD(void,
              GenerateKeyPair,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               const std::vector<uint8_t>& public_attributes,
               const std::vector<uint8_t>& private_attributes,
               GenerateKeyPairCallback callback),
              (override));
  MOCK_METHOD(void,
              WrapKey,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t wrapping_key_handle,
               uint64_t key_handle,
               uint64_t max_out_length,
               DataCallback callback),
              (override));
  MOCK_METHOD(void,
              UnwrapKey,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t wrapping_key_handle,
               const std::vector<uint8_t>& wrapped_key,
               const std::vector<uint8_t>& attributes,
               Uint64Callback callback),
              (override));
  MOCK_METHOD(void,
              DeriveKey,
              (uint64_t session_id,
               uint64_t mechanism_type,
               const std::vector<uint8_t>& mechanism_parameter,
               uint64_t base_key_handle,
               const std::vector<uint8_t>& attributes,
               Uint64Callback callback),
              (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_MOCK_CHAPS_CLIENT_H_
