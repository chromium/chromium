// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_KEY_PROVIDER_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_KEY_PROVIDER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace os_crypt_async {

// KeyProvider is an interface used by OSCryptAsync to provide
// encryption keys for the Encyptor instance. It is not used for data
// encryption, but just for cryptographic operations and storage related to key
// management.
//
// KeyProvider implementations are passed into the constructor of the
// `OSCryptAsync` class.
class KeyProvider {
 public:
  using KeyCallback =
      base::OnceCallback<void(/*tag=*/const std::string&,
                              /*key=*/std::optional<Encryptor::Key>)>;

  virtual ~KeyProvider() = default;

  // Not copyable or movable.
  KeyProvider(const KeyProvider&) = delete;
  KeyProvider& operator=(const KeyProvider&) = delete;
  KeyProvider(KeyProvider&&) = delete;
  KeyProvider& operator=(KeyProvider&&) = delete;

  // The provider should return a non-empty tag value and a valid Key on the
  // `callback`. This Key will be used for cryptographic operations. The `tag`
  // will be used to identify that data has been previously encrypted with the
  // `key` and is typically a constant for a specified provider. The provider
  // should return the same key and tag when running within the same execution
  // context (typically: same OS user, or same running application). It is
  // typically backed by persistent storage, or OS provided, or a combination of
  // the two. The provider should generate the key, if necessary.
  virtual void GetKey(KeyCallback callback) = 0;

  // The provider should return `true` if the Key it provides should be
  // considered valid for encryption of new data. Regardless of the return
  // value, the provider's key will still be considered valid for decryption of
  // data encrypted previously with the key.
  virtual bool UseForEncryption() = 0;

  // Key providers should return whether or not their data is compatible with
  // OSCrypt sync, specifically whether encryption of data with the key and
  // algorithm returned from this provider could be successfully decrypted using
  // Decrypt in os_crypt/sync.
  virtual bool IsCompatibleWithOsCryptSync() = 0;

 protected:
  KeyProvider() = default;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_KEY_PROVIDER_H_
