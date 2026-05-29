// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_MOJOM_TRAITS_H_
#define COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_MOJOM_TRAITS_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/encryptor.mojom.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<os_crypt_async::mojom::EncryptorDataView,
                    scoped_refptr<os_crypt_async::Encryptor>> {
  // Since scoped_refptr is the cpp type, the nullable_is_same_type is true for
  // the trait, and as a result implementations for IsNull and SetToNull are
  // needed here.
  static bool IsNull(const scoped_refptr<os_crypt_async::Encryptor>& in) {
    return !in;
  }

  static void SetToNull(scoped_refptr<os_crypt_async::Encryptor>* out) {
    out->reset();
  }

  static bool Read(os_crypt_async::mojom::EncryptorDataView data,
                   scoped_refptr<os_crypt_async::Encryptor>* out);

  static const std::string& provider_for_encryption(
      const scoped_refptr<const os_crypt_async::Encryptor>& in);
  static const std::map<std::string,
                        std::optional<os_crypt_async::Encryptor::Key>>&
  key_entries(const scoped_refptr<const os_crypt_async::Encryptor>& in);
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
