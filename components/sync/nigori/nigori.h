// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/time/tick_clock.h"
#include "components/sync/base/passphrase_enums.h"

namespace crypto {
class SymmetricKey;
}  // namespace crypto

namespace syncer {

class Nigori;

// TODO(crbug.com/922900): inline kNigoriKeyName into Nigori::Permute().
extern const char kNigoriKeyName[];

class KeyDerivationParams {
 public:
  static KeyDerivationParams CreateForPbkdf2();
  static KeyDerivationParams CreateForScrypt(const std::string& salt);
  static KeyDerivationParams CreateWithUnsupportedMethod();

  KeyDerivationMethod method() const { return method_; }
  const std::string& scrypt_salt() const;

  KeyDerivationParams(const KeyDerivationParams& other);
  KeyDerivationParams(KeyDerivationParams&& other);
  KeyDerivationParams& operator=(const KeyDerivationParams& other);
  bool operator==(const KeyDerivationParams& other) const;
  bool operator!=(const KeyDerivationParams& other) const;

 private:
  KeyDerivationParams(KeyDerivationMethod method,
                      const std::string& scrypt_salt);

  KeyDerivationMethod method_;

  std::string scrypt_salt_;
};

// Enumeration of possible values for a key derivation method (including a
// special value of "not set"). Used in UMA metrics. Do not re-order or delete
// these entries; they are used in a UMA histogram.  Please edit
// SyncCustomPassphraseKeyDerivationMethodState in enums.xml if a value is
// added.
enum class KeyDerivationMethodStateForMetrics {
  NOT_SET = 0,
  UNSUPPORTED = 1,
  PBKDF2_HMAC_SHA1_1003 = 2,
  SCRYPT_8192_8_11 = 3,
  kMaxValue = SCRYPT_8192_8_11
};

// A (partial) implementation of Nigori, a protocol to securely store secrets in
// the cloud. This implementation does not support server authentication or
// assisted key derivation.
//
// To store secrets securely, use the |Permute| method to derive a lookup name
// for your secret (basically a map key), and |Encrypt| and |Decrypt| to store
// and retrieve the secret.
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

  // Derives a secure lookup name from |type| and |name|. If |hostname|,
  // |username| and |password| are kept constant, a given |type| and |name| pair
  // always yields the same |permuted| value. Note that |permuted| will be
  // Base64 encoded.
  bool Permute(Type type, const std::string& name, std::string* permuted) const;

  // Encrypts |value|. Note that on success, |encrypted| will be Base64
  // encoded.
  bool Encrypt(const std::string& value, std::string* encrypted) const;

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

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_H_
