// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor_mojom_traits.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/encryptor.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dpapi.h>

#include "base/feature_list.h"
#include "components/os_crypt/async/common/encryptor_features.h"
#endif

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
  std::optional<size_t> key_size;
  switch (data.algorithm()) {
    case os_crypt_async::mojom::Algorithm::kAES256GCM:
      key_size.emplace(os_crypt_async::Encryptor::Key::kAES256GCMKeySize);
  }

  if (!key_size.has_value()) {
    return false;
  }

  // Using shared memory here means that the key never transits any mojo
  // buffers, but is completely controlled by the traits.
  base::UnsafeSharedMemoryRegion key_memory;
  if (!data.ReadKey(&key_memory)) {
    return false;
  }

  if (key_memory.GetSize() != *key_size) {
    return false;
  }

  out->key_.resize(*key_size);
  auto mapping = key_memory.Map();
  if (!mapping.IsValid()) {
    return false;
  }

  auto memory_span = mapping.GetMemoryAsSpan<uint8_t>();
  std::copy(memory_span.begin(), memory_span.end(), out->key_.begin());

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          os_crypt_async::features::kProtectEncryptionKey)) {
    SecureZeroMemory(std::data(memory_span), std::size(memory_span));
    out->encrypted_ =
        ::CryptProtectMemory(std::data(out->key_), std::size(out->key_),
                             CRYPTPROTECTMEMORY_SAME_PROCESS);
  }
#endif  // BUILDFLAG(IS_WIN)

  out->algorithm_ = data.algorithm();

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
  NOTREACHED();
}

// static
base::UnsafeSharedMemoryRegion StructTraits<os_crypt_async::mojom::KeyDataView,
                                            os_crypt_async::Encryptor::Key>::
    key(const os_crypt_async::Encryptor::Key& in) {
  auto region = base::UnsafeSharedMemoryRegion::Create(in.key_.size());
  auto mapping = region.Map();
  auto memory_span = mapping.GetMemoryAsSpan<uint8_t>();
  memory_span.copy_from(in.key_);
#if BUILDFLAG(IS_WIN)
  if (in.encrypted_) {
    // Not much we can do if this fails.
    std::ignore =
        ::CryptUnprotectMemory(std::data(memory_span), std::size(memory_span),
                               CRYPTPROTECTMEMORY_SAME_PROCESS);
  }
#endif  // BUILDFLAG(IS_WIN)
  return region;
}

}  // namespace mojo
