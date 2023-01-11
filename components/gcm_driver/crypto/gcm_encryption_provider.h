// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

namespace gcm {

enum class GCMDecryptionResult;
enum class GCMEncryptionResult;
class GCMKeyStore;
struct IncomingMessage;

// Provider that enables the GCM Driver to deal with encryption key management
// and decryption of incoming messages.
class GCMEncryptionProvider {
 public:
  // Callback to be invoked when the public key and auth secret are available.
  using EncryptionInfoCallback =
      base::OnceCallback<void(std::string p256dh, std::string auth_secret)>;

  // Callback to be invoked when a message may have been decrypted, as indicated
  // by the |result|. The |message| contains the dispatchable message in success
  // cases, or will be initialized to an empty, default state for failure.
  using DecryptMessageCallback =
      base::OnceCallback<void(GCMDecryptionResult result,
                              IncomingMessage message)>;

  // Callback to be invoked when a message may have been encrypted, as indicated
  // by the |result|. The |message| contains the dispatchable message in success
  // cases, or will be initialized to an empty, default state for failure.
  using EncryptMessageCallback =
      base::OnceCallback<void(GCMEncryptionResult result, std::string message)>;

  static const char kContentEncodingProperty[];

  // Content coding name defined by ietf-httpbis-encryption-encoding.
  static const char kContentCodingAes128Gcm[];

  GCMEncryptionProvider();

  GCMEncryptionProvider(const GCMEncryptionProvider&) = delete;
  GCMEncryptionProvider& operator=(const GCMEncryptionProvider&) = delete;

  ~GCMEncryptionProvider();

  // Initializes the encryption provider with the |store_path| and the
  // |blocking_task_runner|. Done separately from the constructor in order to
  // avoid needing a blocking task runner for anything using GCMDriver.
  void Init(
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  // Retrieves the public key and authentication secret associated with the
  // |app_id| + |authorized_entity| pair. Will create this info if necessary.
  // |authorized_entity| should be the InstanceID token's authorized entity, or
  // "" for non-InstanceID GCM registrations.
  void GetEncryptionInfo(const std::string& app_id,
                         const std::string& authorized_entity,
                         EncryptionInfoCallback callback);

  // Removes all encryption information associated with the |app_id| +
  // |authorized_entity| pair, then invokes |callback|. |authorized_entity|
  // should be the InstanceID token's authorized entity, or "*" to remove for
  // all InstanceID tokens, or "" for non-InstanceID GCM registrations.
  void RemoveEncryptionInfo(const std::string& app_id,
                            const std::string& authorized_entity,
                            base::OnceClosure callback);

  // Determines whether |message| contains encrypted content.
  bool IsEncryptedMessage(const IncomingMessage& message) const;

  // Attempts to decrypt the |message|. If the |message| is not encrypted, the
  // |callback| will be invoked immediately. Otherwise |callback| will be called
  // asynchronously when |message| has been decrypted. A dispatchable message
  // will be used in case of success, an empty message in case of failure.
  void DecryptMessage(const std::string& app_id,
                      const IncomingMessage& message,
                      DecryptMessageCallback callback);

  // Attempts to encrypt the |message| using draft-ietf-webpush-encryption-08
  // scheme. |callback| will be called asynchronously when |message| has been
  // encrypted. A dispatchable message will be used in case of success, an empty
  // message in case of failure.
  void EncryptMessage(const std::string& app_id,
                      const std::string& authorized_entity,
                      const std::string& p256dh,
                      const std::string& auth_secret,
                      const std::string& message,
                      EncryptMessageCallback callback);

 private:
  friend class GCMEncryptionProviderTest;
  FRIEND_TEST_ALL_PREFIXES(GCMEncryptionProviderTest,
                           EncryptionRoundTripGCMRegistration);
  FRIEND_TEST_ALL_PREFIXES(GCMEncryptionProviderTest,
                           EncryptionRoundTripInstanceIDToken);

  void DidGetEncryptionInfo(const std::string& app_id,
                            const std::string& authorized_entity,
                            EncryptionInfoCallback callback,
                            std::unique_ptr<crypto::ECPrivateKey> key,
                            const std::string& auth_secret);

  void DidCreateEncryptionInfo(EncryptionInfoCallback callback,
                               std::unique_ptr<crypto::ECPrivateKey> key,
                               const std::string& auth_secret);

  void DecryptMessageWithKey(const std::string& message_id,
                             const std::string& collapse_key,
                             const std::string& sender_id,
                             const std::string& salt,
                             const std::string& public_key,
                             uint32_t record_size,
                             const std::string& ciphertext,
                             GCMMessageCryptographer::Version version,
                             DecryptMessageCallback callback,
                             std::unique_ptr<crypto::ECPrivateKey> key,
                             const std::string& auth_secret);

  void EncryptMessageWithKey(const std::string& app_id,
                             const std::string& authorized_entity,
                             const std::string& p256dh,
                             const std::string& auth_secret,
                             const std::string& message,
                             EncryptMessageCallback callback,
                             std::unique_ptr<crypto::ECPrivateKey> key,
                             const std::string& sender_auth_secret);

  std::unique_ptr<GCMKeyStore> key_store_;

  base::WeakPtrFactory<GCMEncryptionProvider> weak_ptr_factory_{this};
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_ENCRYPTION_PROVIDER_H_
