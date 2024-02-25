// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_CHAPS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_CHAPS_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace ash {

// Interface for communicating with the Chaps daemon over D-Bus. It should be
// kept in sync with platform2/chaps/dbus_bindings/org.chromium.Chaps.xml .
// The class is exported for unit tests, use SimpleChapsClient for communicating
// with Chaps.
class COMPONENT_EXPORT(ASH_DBUS_CHAPS) ChapsClient {
 public:
  // Callback types.
  using ResultCodeCallback = base::OnceCallback<void(uint32_t result_code)>;
  using Uint64Callback =
      base::OnceCallback<void(uint64_t value, uint32_t result_code)>;
  using ArrayOfUint64Callback =
      base::OnceCallback<void(const std::vector<uint64_t>& list,
                              uint32_t result_code)>;
  using DataCallback = base::OnceCallback<void(uint64_t actual_out_length,
                                               const std::vector<uint8_t>& data,
                                               uint32_t result_code)>;
  // `attributes` are a serialized chaps::AttributeList.
  using GetAttributeValueCallback =
      base::OnceCallback<void(const std::vector<uint8_t>& attributes,
                              uint32_t result_code)>;
  using GenerateKeyPairCallback =
      base::OnceCallback<void(uint64_t public_key_handle,
                              uint64_t private_key_handle,
                              uint32_t result_code)>;

  ChapsClient(const ChapsClient&) = delete;
  ChapsClient& operator=(const ChapsClient&) = delete;
  ChapsClient(ChapsClient&&) = delete;
  ChapsClient& operator=(ChapsClient&&) = delete;

  // Returns the global instance which may be null if not initialized.
  static ChapsClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // PKCS #11 v2.20 section 11.5 page 106.
  virtual void GetSlotList(bool token_present,
                           ArrayOfUint64Callback callback) = 0;
  // PKCS #11 v2.20 section 11.5 page 111.
  virtual void GetMechanismList(uint64_t slot_id,
                                ArrayOfUint64Callback callback) = 0;
  // PKCS #11 v2.20 section 11.6 page 117.
  virtual void OpenSession(uint64_t slot_id,
                           uint64_t flags,
                           Uint64Callback callback) = 0;
  // PKCS #11 v2.20 section 11.6 page 118.
  virtual void CloseSession(uint64_t session_id,
                            ResultCodeCallback callback) = 0;
  // PKCS #11 v2.20 section 11.7 page 128.
  virtual void CreateObject(uint64_t session_id,
                            const std::vector<uint8_t>& attributes,
                            Uint64Callback callback) = 0;
  // PKCS #11 v2.20 section 11.7 page 131.
  virtual void DestroyObject(uint64_t session_id,
                             uint64_t object_handle,
                             ResultCodeCallback callback) = 0;
  // PKCS #11 v2.20 section 11.7 page 133.
  virtual void GetAttributeValue(uint64_t session_id,
                                 uint64_t object_handle,
                                 const std::vector<uint8_t>& attributes_query,
                                 GetAttributeValueCallback callback) = 0;
  // PKCS #11 v2.20 section 11.7 page 135.
  virtual void SetAttributeValue(uint64_t session_id,
                                 uint64_t object_handle,
                                 const std::vector<uint8_t>& attributes,
                                 ResultCodeCallback callback) = 0;
  // PKCS #11 v2.20 section 11.7 page 136.
  virtual void FindObjectsInit(uint64_t session_id,
                               const std::vector<uint8_t>& attributes,
                               ResultCodeCallback callback) = 0;
  // PKCS #11 v2.20 section 11.7 page 137.
  virtual void FindObjects(uint64_t session_id,
                           uint64_t max_object_count,
                           ArrayOfUint64Callback callback) = 0;
  // PKCS #11 v2.20 section 11.7 page 138.
  virtual void FindObjectsFinal(uint64_t session_id,
                                ResultCodeCallback callback) = 0;
  // PKCS #11 v2.20 section 11.8 page 139.
  virtual void EncryptInit(uint64_t session_id,
                           uint64_t mechanism_type,
                           const std::vector<uint8_t>& mechanism_parameter,
                           uint64_t key_handle,
                           ResultCodeCallback callback) = 0;
  // PKCS #11 v2.20 section 11.8 page 140.
  virtual void Encrypt(uint64_t session_id,
                       const std::vector<uint8_t>& data,
                       uint64_t max_out_length,
                       DataCallback callback) = 0;
  // PKCS #11 v2.20 section 11.9 page 144.
  virtual void DecryptInit(uint64_t session_id,
                           uint64_t mechanism_type,
                           const std::vector<uint8_t>& mechanism_parameter,
                           uint64_t key_handle,
                           ResultCodeCallback callback) = 0;
  // PKCS #11 v2.20 section 11.9 page 145.
  virtual void Decrypt(uint64_t session_id,
                       const std::vector<uint8_t>& data,
                       uint64_t max_out_length,
                       DataCallback callback) = 0;
  // PKCS #11 v2.20 section 11.11 page 152.
  virtual void SignInit(uint64_t session_id,
                        uint64_t mechanism_type,
                        const std::vector<uint8_t>& mechanism_parameter,
                        uint64_t key_handle,
                        ResultCodeCallback callback) = 0;
  // PKCS #11 v2.20 section 11.11 page 153.
  virtual void Sign(uint64_t session_id,
                    const std::vector<uint8_t>& data,
                    uint64_t max_out_length,
                    DataCallback callback) = 0;
  // PKCS #11 v2.20 section 11.14 page 176.
  virtual void GenerateKeyPair(uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               const std::vector<uint8_t>& public_attributes,
                               const std::vector<uint8_t>& private_attributes,
                               GenerateKeyPairCallback callback) = 0;
  // PKCS #11 v2.20 section 11.14 page 178.
  virtual void WrapKey(uint64_t session_id,
                       uint64_t mechanism_type,
                       const std::vector<uint8_t>& mechanism_parameter,
                       uint64_t wrapping_key_handle,
                       uint64_t key_handle,
                       uint64_t max_out_length,
                       DataCallback callback) = 0;
  // PKCS #11 v2.20 section 11.14 page 180.
  virtual void UnwrapKey(uint64_t session_id,
                         uint64_t mechanism_type,
                         const std::vector<uint8_t>& mechanism_parameter,
                         uint64_t wrapping_key_handle,
                         const std::vector<uint8_t>& wrapped_key,
                         const std::vector<uint8_t>& attributes,
                         Uint64Callback callback) = 0;
  // PKCS #11 v2.20 section 11.14 page 182.
  virtual void DeriveKey(uint64_t session_id,
                         uint64_t mechanism_type,
                         const std::vector<uint8_t>& mechanism_parameter,
                         uint64_t base_key_handle,
                         const std::vector<uint8_t>& attributes,
                         Uint64Callback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  ChapsClient();
  virtual ~ChapsClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CHAPS_CHAPS_CLIENT_H_
