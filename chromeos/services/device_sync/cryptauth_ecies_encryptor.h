// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_H_

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"

namespace chromeos {

namespace device_sync {

// Encrypts/decrypts strings using the Elliptic Curve Integrated Encryption
// Scheme (ECIES).
//
// A CryptAuthEciesEncryptor object is designed to be used for only one method
// call. To perform another encryption or decryption, a new object should be
// created.
class CryptAuthEciesEncryptor {
 public:
  struct PayloadAndKey {
    PayloadAndKey();
    PayloadAndKey(const std::string& payload, const std::string& key);
    bool operator==(const PayloadAndKey& other) const;

    // Unencrypted/Encrypted payload to be encrypted/decrypted.
    std::string payload;

    // Public/Private key for encryption/decryption.
    std::string key;
  };

  using IdToInputMap = base::flat_map<std::string, PayloadAndKey>;
  using IdToOutputMap =
      base::flat_map<std::string, base::Optional<std::string>>;
  using SingleInputCallback =
      base::OnceCallback<void(const base::Optional<std::string>&)>;
  using BatchCallback = base::OnceCallback<void(const IdToOutputMap&)>;

  virtual ~CryptAuthEciesEncryptor();

  // Encrypts/Decrypts the input payload with the provided key, returning the
  // encrypted/decrypted payload in the callback or null if the
  // encryption/decryption was unsuccessful.
  void Encrypt(const std::string& unencrypted_payload,
               const std::string& encrypting_public_key,
               SingleInputCallback encryption_finished_callback);
  void Decrypt(const std::string& encrypted_payload,
               const std::string& decrypting_private_key,
               SingleInputCallback decryption_finished_callback);

  // Encrypts/Decrypts all payloads of the input ID-to-PayloadAndKey map with
  // the provided keys. The encrypted/decrypted payloads are returned in the
  // callback, paired with their input IDs. Unsuccessful encryptions/decryptions
  // result in null values. Note: The IDs are only used to correlate the output
  // with input.
  void BatchEncrypt(const IdToInputMap& id_to_payload_and_key_map,
                    BatchCallback encryption_finished_callback);
  void BatchDecrypt(const IdToInputMap& id_to_payload_and_key_map,
                    BatchCallback decryption_finished_callback);

 protected:
  CryptAuthEciesEncryptor();

  virtual void OnBatchEncryptionStarted() = 0;
  virtual void OnBatchDecryptionStarted() = 0;

  void OnAttemptFinished(const IdToOutputMap& id_to_output_map);

  IdToInputMap id_to_input_map_;

 private:
  void ProcessInput(const IdToInputMap& id_to_input_map,
                    BatchCallback callback);

  BatchCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEciesEncryptor);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_H_
