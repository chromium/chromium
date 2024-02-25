// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor.h"

namespace securemessage {
class Header;
}  // namespace securemessage

namespace ash {

namespace multidevice {
class SecureMessageDelegate;
}

namespace device_sync {

// An implementation of CryptAuthEciesEncryptor using the SecureMessage library.
//
// The encryption algorithm is as follows:
//   1) Generate a session key pair.
//   2) Use Diffie-Hellman key exchange to derive symmetric key from the session
//      private key and the input encrypting public key.
//   3) Create a SecureMessage from the input payload, encrypted and signed with
//      the derived symmetric key, using AES-256-CBC and HMAC-SHA256,
//      respectively.
//   4) Include the session public key in the unencrypted SecureMessage Header's
//      decryption_key_id field.
//   5) Output the SecureMessage serialized as a string.
//
// The decryption algorithm is as follows:
//   1) Parse the serialized SecureMessage input.
//   2) Use Diffie-Hellman key exchange to derive symmetric key from the session
//      public key--provided in the unencrypted SecureMessage Header's
//      decryption_key_id field--and the input decrypting private key. This
//      should match the key derived in the step 2) of the encryption algorithm
//      if the encrypting public key and decrypting private key are a key pair.
//   3) Decrypt the input SecureMessage and verify the signature using the
//      derived symmetric key.
//   4) Extract the decrypted payload string from the body of the decrypted
//      SecureMessage.
//
// Note that the input encrypting public keys must be a serialized
// securemessage::GenericPublicKey proto, and the encrypting/decrypting key pair
// must be P-256.
class CryptAuthEciesEncryptorImpl : public CryptAuthEciesEncryptor {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthEciesEncryptor> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthEciesEncryptor> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  CryptAuthEciesEncryptorImpl(const CryptAuthEciesEncryptorImpl&) = delete;
  CryptAuthEciesEncryptorImpl& operator=(const CryptAuthEciesEncryptorImpl&) =
      delete;

  ~CryptAuthEciesEncryptorImpl() override;

 private:
  CryptAuthEciesEncryptorImpl();

  // CryptAuthEciesEncryptor:
  void OnBatchEncryptionStarted() override;
  void OnBatchDecryptionStarted() override;

  void OnSingleOutputFinished(const std::string& id,
                              const std::optional<std::string>& output);
  void OnSessionKeyPairGenerated(const std::string& session_public_key,
                                 const std::string& session_private_key);
  void OnDiffieHellmanEncryptionKeyDerived(
      const std::string& id,
      const std::string& session_public_key,
      const std::string& dh_key);
  void OnSecureMessageCreated(
      const std::string& id,
      const std::string& serialized_encrypted_secure_message);
  void OnDiffieHellmanDecryptionKeyDerived(
      const std::string& id,
      const std::string& serialized_encrypted_secure_message,
      const std::string& dh_key);
  void OnSecureMessageUnwrapped(const std::string& id,
                                bool verified,
                                const std::string& payload,
                                const securemessage::Header& header);

  size_t remaining_batch_size_ = 0;
  IdToOutputMap id_to_output_map_;
  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ECIES_ENCRYPTOR_IMPL_H_
