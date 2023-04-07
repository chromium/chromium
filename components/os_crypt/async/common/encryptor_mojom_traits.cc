// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor_mojom_traits.h"

#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/encryptor.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

#include <map>
#include <string>
#include <vector>

namespace mojo {

// static
bool StructTraits<os_crypt_async::mojom::EncryptorDataView,
                  os_crypt_async::Encryptor>::
    Read(os_crypt_async::mojom::EncryptorDataView data,
         os_crypt_async::Encryptor* out) {
  if (!data.ReadProviderForEncryption(&out->provider_for_encryption_)) {
    return false;
  }

  if (!data.ReadKeyEntries(&out->keys_)) {
    return false;
  }

  return true;
}

// static
const std::string& StructTraits<os_crypt_async::mojom::EncryptorDataView,
                                os_crypt_async::Encryptor>::
    provider_for_encryption(const os_crypt_async::Encryptor& in) {
  return in.provider_for_encryption_;
}

// static
const std::map<std::string, os_crypt_async::Encryptor::Key>& StructTraits<
    os_crypt_async::mojom::EncryptorDataView,
    os_crypt_async::Encryptor>::key_entries(const os_crypt_async::Encryptor&
                                                in) {
  return in.keys_;
}

// static
bool StructTraits<os_crypt_async::mojom::KeyDataView,
                  os_crypt_async::Encryptor::Key>::
    Read(os_crypt_async::mojom::KeyDataView data,
         os_crypt_async::Encryptor::Key* out) {
  out->algorithm_ = data.algorithm();

  if (!data.ReadKey(&out->key_)) {
    return false;
  }

  switch (*out->algorithm_) {
    case os_crypt_async::mojom::Algorithm::kAES256GCM:
      if (out->key_.size() !=
          os_crypt_async::Encryptor::Key::kAES256GCMKeySize) {
        return false;
      }
      break;
  }

  return true;
}

// static
const os_crypt_async::mojom::Algorithm&
StructTraits<os_crypt_async::mojom::KeyDataView,
             os_crypt_async::Encryptor::Key>::
    algorithm(const os_crypt_async::Encryptor::Key& in) {
  if (in.algorithm_) {
    return *in.algorithm_;
  }
  NOTREACHED_NORETURN();
}

// static
const std::vector<uint8_t>& StructTraits<os_crypt_async::mojom::KeyDataView,
                                         os_crypt_async::Encryptor::Key>::
    key(const os_crypt_async::Encryptor::Key& in) {
  return in.key_;
}

}  // namespace mojo
