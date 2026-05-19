// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_REQUIRED_PASSPHRASE_VERIFIER_IMPL_H_
#define COMPONENTS_SYNC_NIGORI_REQUIRED_PASSPHRASE_VERIFIER_IMPL_H_

#include <memory>
#include <string>

#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/required_passphrase_verifier.h"
#include "components/sync/protocol/encryption.pb.h"

namespace syncer {

class CustomPassphraseBootstrapToken;

class RequiredPassphraseVerifierImpl : public RequiredPassphraseVerifier {
 public:
  RequiredPassphraseVerifierImpl(KeyDerivationParams derivation_params,
                                 sync_pb::EncryptedData pending_keys);

  RequiredPassphraseVerifierImpl(const RequiredPassphraseVerifierImpl&) =
      delete;
  RequiredPassphraseVerifierImpl& operator=(
      const RequiredPassphraseVerifierImpl&) = delete;

  ~RequiredPassphraseVerifierImpl() override;

  // RequiredPassphraseVerifier:
  bool IsValidDecryptionPassphrase(const std::string& passphrase) override;
  bool IsValidDecryptionBootstrapToken(
      const CustomPassphraseBootstrapToken& bootstrap_token) override;
  std::unique_ptr<RequiredPassphraseVerifier> Clone() const override;

 private:
  const KeyDerivationParams derivation_params_;
  const sync_pb::EncryptedData pending_keys_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_REQUIRED_PASSPHRASE_VERIFIER_IMPL_H_
