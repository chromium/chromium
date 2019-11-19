// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_key_creator.h"

namespace chromeos {

namespace multidevice {
class SecureMessageDelegate;
}  // namespace multidevice

namespace device_sync {

// Implementation of CryptAuthKeyCreator.
class CryptAuthKeyCreatorImpl : public CryptAuthKeyCreator {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthKeyCreator> BuildInstance();

   private:
    static Factory* test_factory_;
  };

  ~CryptAuthKeyCreatorImpl() override;

  // CryptAuthKeyCreator:
  void CreateKeys(const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
                      keys_to_create,
                  const base::Optional<CryptAuthKey>& server_ephemeral_dh,
                  CreateKeysCallback create_keys_callback) override;

 private:
  CryptAuthKeyCreatorImpl();

  void OnClientDiffieHellmanGenerated(const std::string& public_key,
                                      const std::string& private_key);
  void OnDiffieHellmanHandshakeSecretDerived(const std::string& symmetric_key);
  void StartKeyCreation();
  void OnAsymmetricKeyPairGenerated(CryptAuthKeyBundle::Name bundle_name,
                                    const std::string& public_key,
                                    const std::string& private_key);
  void OnSymmetricKeyDerived(CryptAuthKeyBundle::Name bundle_name,
                             const std::string& symmetric_key);

  size_t num_keys_to_create_ = 0;
  base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData> keys_to_create_;
  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey> new_keys_;
  base::Optional<CryptAuthKey> server_ephemeral_dh_;
  base::Optional<CryptAuthKey> client_ephemeral_dh_;
  base::Optional<CryptAuthKey> dh_handshake_secret_;
  CreateKeysCallback create_keys_callback_;

  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyCreatorImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_IMPL_H_
