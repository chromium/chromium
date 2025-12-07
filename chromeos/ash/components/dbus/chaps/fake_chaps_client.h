// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_FAKE_CHAPS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_FAKE_CHAPS_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/dbus/chaps/chaps_client.h"

namespace ash {

// A fake version of ChapsClient. Does not simulate the real Chaps very
// precisely. When implementing new features, do not blindly rely on this class
// behaving correctly and add required functionality as needed. See comments in
// the .cc file for known simplifications.
class COMPONENT_EXPORT(ASH_DBUS_CHAPS) FakeChapsClient : public ChapsClient {
 public:
  FakeChapsClient();
  ~FakeChapsClient() override;

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

  // Methods below are a part of the "for testing" interface of this fake
  // object.

  // If a value is passed, the `OpenSession` method will start returning it as
  // an error. If a nullopt is passed, `OpenSession` will be reset to the
  // default behavior.
  void SimulateOpenSessionError(std::optional<uint32_t> result_code);
  // Get the current counter of how many times `OpenSession` was called and
  // reset it to 0.
  int GetAndResetOpenSessionCounter();

 private:
  using ObjectMap = base::flat_map<uint64_t /*object_id*/,
                                   std::vector<uint8_t> /*object_attrs*/>;

  struct ObjectLocation {
    uint64_t slot_id;
    ObjectMap::iterator iterator;
  };

  base::expected<ObjectLocation, uint32_t /*result_code*/> FindObject(
      uint64_t session_id,
      uint64_t object_handle);

  uint64_t next_session_id_ = 1;
  base::flat_map<uint64_t /*session_id*/, uint64_t /*slot_id*/> open_sessions_;

  uint64_t next_object_id_ = 1;

  base::flat_map<uint64_t /*slot_id*/, ObjectMap> stored_objects_;

  base::flat_map<uint64_t /*session_id*/, bool> find_objects_init_called_;
  base::flat_map<uint64_t /*session_id*/, bool> sign_init_called_;

  std::optional<uint32_t> simulated_open_session_result_code_;
  size_t open_session_call_counter_ = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_FAKE_CHAPS_CLIENT_H_
