// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_MOJOM_TRAITS_H_
#define COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_MOJOM_TRAITS_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/encryptor.mojom.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<os_crypt_async::mojom::EncryptorDataView,
                    os_crypt_async::Encryptor> {
  static bool Read(os_crypt_async::mojom::EncryptorDataView data,
                   os_crypt_async::Encryptor* out);

  static const std::string& provider_for_encryption(
      const os_crypt_async::Encryptor& in);
  static const std::map<std::string, os_crypt_async::Encryptor::Key>&
  key_entries(const os_crypt_async::Encryptor& in);
};

template <>
struct StructTraits<os_crypt_async::mojom::KeyDataView,
                    os_crypt_async::Encryptor::Key> {
  static bool Read(os_crypt_async::mojom::KeyDataView data,
                   os_crypt_async::Encryptor::Key* out);

  static const os_crypt_async::mojom::Algorithm& algorithm(
      const os_crypt_async::Encryptor::Key& in);
  static base::UnsafeSharedMemoryRegion key(
      const os_crypt_async::Encryptor::Key& in);
};

}  // namespace mojo

#endif  // COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_MOJOM_TRAITS_H_
