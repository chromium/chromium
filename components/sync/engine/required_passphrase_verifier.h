// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_REQUIRED_PASSPHRASE_VERIFIER_H_
#define COMPONENTS_SYNC_ENGINE_REQUIRED_PASSPHRASE_VERIFIER_H_

#include <memory>
#include <string>

namespace syncer {

class CustomPassphraseBootstrapToken;

// Interface used to verify whether a given passphrase or bootstrap token
// correctly decrypts pending sync keys. Designed to be safely passed across
// threads/sequences via std::unique_ptr.
class RequiredPassphraseVerifier {
 public:
  virtual ~RequiredPassphraseVerifier() = default;

  // Verifies if `passphrase` successfully decrypts pending keys.
  virtual bool IsValidDecryptionPassphrase(const std::string& passphrase) = 0;

  // Verifies if `bootstrap_token` successfully decrypts pending keys.
  virtual bool IsValidDecryptionBootstrapToken(
      const CustomPassphraseBootstrapToken& bootstrap_token) = 0;

  // Creates a deep copy of the verifier for broadcasting to multiple observers.
  virtual std::unique_ptr<RequiredPassphraseVerifier> Clone() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_REQUIRED_PASSPHRASE_VERIFIER_H_
