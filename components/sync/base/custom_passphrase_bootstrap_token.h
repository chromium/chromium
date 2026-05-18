// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_CUSTOM_PASSPHRASE_BOOTSTRAP_TOKEN_H_
#define COMPONENTS_SYNC_BASE_CUSTOM_PASSPHRASE_BOOTSTRAP_TOKEN_H_

#include <string>

#include "components/sync/protocol/nigori_specifics.pb.h"

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace syncer {

// A type-safe wrapper around the encryption bootstrap token (which holds the
// raw Nigori keys derived from a user's custom passphrase or legacy implicit
// passphrase). Encapsulates the unencrypted token state in memory and handles
// serialization and encryption for preferences.
class CustomPassphraseBootstrapToken {
 public:
  static CustomPassphraseBootstrapToken FromEncryptedPref(
      const std::string& encrypted_pref,
      const os_crypt_async::Encryptor& encryptor);

  static CustomPassphraseBootstrapToken FromProto(sync_pb::NigoriKey proto);
  static CustomPassphraseBootstrapToken CreateFakeForTesting(int index = 0);

  CustomPassphraseBootstrapToken();
  ~CustomPassphraseBootstrapToken();

  CustomPassphraseBootstrapToken(const CustomPassphraseBootstrapToken&);
  CustomPassphraseBootstrapToken(CustomPassphraseBootstrapToken&&);
  CustomPassphraseBootstrapToken& operator=(
      const CustomPassphraseBootstrapToken&);
  CustomPassphraseBootstrapToken& operator=(CustomPassphraseBootstrapToken&&);

  bool IsEmpty() const;

  // Serializes the token to a string and encrypts it using `encryptor`,
  // returning a base64-encoded string ready to be stored in preferences.
  // Returns an empty string if the token is empty or encryption fails.
  std::string ToEncryptedPref(const os_crypt_async::Encryptor& encryptor) const;

  const sync_pb::NigoriKey& ToProto() const;

 private:
  explicit CustomPassphraseBootstrapToken(sync_pb::NigoriKey proto);

  sync_pb::NigoriKey proto_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_CUSTOM_PASSPHRASE_BOOTSTRAP_TOKEN_H_
