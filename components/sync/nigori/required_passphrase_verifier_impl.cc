// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/required_passphrase_verifier_impl.h"

#include <memory>
#include <utility>

#include "components/sync/base/custom_passphrase_bootstrap_token.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

RequiredPassphraseVerifierImpl::RequiredPassphraseVerifierImpl(
    KeyDerivationParams derivation_params,
    sync_pb::EncryptedData pending_keys)
    : derivation_params_(std::move(derivation_params)),
      pending_keys_(std::move(pending_keys)) {}

RequiredPassphraseVerifierImpl::~RequiredPassphraseVerifierImpl() = default;

bool RequiredPassphraseVerifierImpl::IsValidDecryptionPassphrase(
    const std::string& passphrase) {
  if (passphrase.empty() || pending_keys_.blob().empty()) {
    return false;
  }
  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(derivation_params_, passphrase);
  if (!nigori) {
    return false;
  }
  std::string plaintext;
  return nigori->Decrypt(pending_keys_.blob(), &plaintext);
}

bool RequiredPassphraseVerifierImpl::IsValidDecryptionBootstrapToken(
    const CustomPassphraseBootstrapToken& bootstrap_token) {
  if (bootstrap_token.IsEmpty() || pending_keys_.blob().empty()) {
    return false;
  }
  const sync_pb::NigoriKey& proto = bootstrap_token.ToProto();
  std::unique_ptr<Nigori> nigori = Nigori::CreateByImport(
      proto.deprecated_user_key(), proto.encryption_key(), proto.mac_key());
  if (!nigori) {
    return false;
  }
  std::string plaintext;
  return nigori->Decrypt(pending_keys_.blob(), &plaintext);
}

std::unique_ptr<RequiredPassphraseVerifier>
RequiredPassphraseVerifierImpl::Clone() const {
  return std::make_unique<RequiredPassphraseVerifierImpl>(derivation_params_,
                                                          pending_keys_);
}

}  // namespace syncer
