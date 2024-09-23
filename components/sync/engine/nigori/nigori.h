// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_NIGORI_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_NIGORI_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/time/tick_clock.h"

namespace crypto {
class SymmetricKey;
}  // namespace crypto

namespace syncer {

class KeyDerivationParams;

// A (partial) implementation of Nigori, a protocol to securely store secrets in
// the cloud. This implementation does not support server authentication or
// assisted key derivation.
//
// To store secrets securely, use the |GetKeyName| method to derive a lookup
// name for your secret (basically a map key), and |Encrypt| and |Decrypt| to
// store and retrieve the secret.
//
// https://www.cl.cam.ac.uk/~drt24/nigori/nigori-overview.pdf
class Nigori {
 public:
  enum Type {
    Password = 1,
  };

  virtual ~Nigori();

  // Initialize by deriving keys based on the given |key_derivation_params| and
  // |password|. The key derivation method must not be UNSUPPORTED. The return
  // value is guaranteed to be non-null.
  static std::unique_ptr<Nigori> CreateByDerivation(
      const KeyDerivationParams& key_derivation_params,
      const std::string& password);

  // Initialize by importing the given keys instead of deriving new ones.
  // Returns null in case of failure.
  static std::unique_ptr<Nigori> CreateByImport(
      const std::string& user_key,
      const std::string& encryption_key,
      const std::string& mac_key);

  // Derives a secure lookup name for |this|, computed as
  // Permute[Kenc,Kmac](Nigori::Password || "nigori-key") as per Nigori
  // protocol. The return value will be Base64 encoded.
  std::string GetKeyName() const;

  // Encrypts |value|. Note that the returned value is Base64 encoded.
  std::string Encrypt(const std::string& value) const;

  // Decrypts |value| into |decrypted|. It is assumed that |value| is Base64
  // encoded.
  bool Decrypt(const std::string& value, std::string* decrypted) const;

  // Exports the raw derived keys.
  void ExportKeys(std::string* user_key,
                  std::string* encryption_key,
                  std::string* mac_key) const;

  // Same as CreateByDerivation() but allows overriding the clock.
  static std::unique_ptr<Nigori> CreateByDerivationForTesting(
      const KeyDerivationParams& key_derivation_params,
      const std::string& password,
      const base::TickClock* tick_clock);

  static std::string GenerateScryptSalt();

  // Allows tests to use faster key derivation with Scrypt derivation method.
  static void SetUseScryptCostParameterForTesting(bool use_low_scrypt_cost);

  // Exposed for tests.
  static const size_t kIvSize = 16;

 private:
  struct Keys {
    Keys();
    ~Keys();

    // TODO(vitaliii): user_key isn't used any more, but legacy clients will
    // fail to import a nigori node without one. We preserve it for the sake of
    // those clients, but it should be removed once enough clients have upgraded
    // to code that doesn't enforce its presence.
    std::unique_ptr<crypto::SymmetricKey> user_key;
    std::unique_ptr<crypto::SymmetricKey> encryption_key;
    std::unique_ptr<crypto::SymmetricKey> mac_key;

    void InitByDerivationUsingPbkdf2(const std::string& password);
    void InitByDerivationUsingScrypt(const std::string& salt,
                                     const std::string& password);
    bool InitByImport(const std::string& user_key_str,
                      const std::string& encryption_key_str,
                      const std::string& mac_key_str);
  };

  Nigori();

  static std::unique_ptr<Nigori> CreateByDerivationImpl(
      const KeyDerivationParams& key_derivation_params,
      const std::string& password,
      const base::TickClock* tick_clock);

  Keys keys_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_NIGORI_H_
