// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_creator.h"

namespace ash {

namespace multidevice {
class SecureMessageDelegate;
}

namespace device_sync {

// Implementation of CryptAuthKeyCreator.
class CryptAuthKeyCreatorImpl : public CryptAuthKeyCreator {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthKeyCreator> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthKeyCreator> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  CryptAuthKeyCreatorImpl(const CryptAuthKeyCreatorImpl&) = delete;
  CryptAuthKeyCreatorImpl& operator=(const CryptAuthKeyCreatorImpl&) = delete;

  ~CryptAuthKeyCreatorImpl() override;

  // CryptAuthKeyCreator:
  void CreateKeys(const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
                      keys_to_create,
                  const std::optional<CryptAuthKey>& server_ephemeral_dh,
                  CreateKeysCallback create_keys_callback) override;

 private:
  CryptAuthKeyCreatorImpl();

  void OnClientDiffieHellmanGenerated(const CryptAuthKey& server_ephemeral_dh,
                                      const std::string& public_key,
                                      const std::string& private_key);
  void OnDiffieHellmanHandshakeSecretDerived(const std::string& symmetric_key);

  // The Diffie-Hellman handshake secret, derived from the ephemeral server and
  // client keys, is null if no symmetric keys need to be created or if there
  // was an error deriving the handshake secret.
  void StartKeyCreation(const std::optional<CryptAuthKey>& dh_handshake_secret);

  void OnAsymmetricKeyPairGenerated(CryptAuthKeyBundle::Name bundle_name,
                                    const std::string& public_key,
                                    const std::string& private_key);
  void OnSymmetricKeyDerived(CryptAuthKeyBundle::Name bundle_name,
                             const std::string& symmetric_key);

  size_t num_keys_to_create_ = 0;
  base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData> keys_to_create_;
  base::flat_map<CryptAuthKeyBundle::Name, std::optional<CryptAuthKey>>
      new_keys_;
  std::optional<CryptAuthKey> client_ephemeral_dh_;
  CreateKeysCallback create_keys_callback_;

  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_IMPL_H_
