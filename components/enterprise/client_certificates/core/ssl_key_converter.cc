// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ssl_key_converter.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "net/ssl/openssl_private_key.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

#if BUILDFLAG(IS_WIN)
#include "net/ssl/ssl_platform_key_win.h"
#elif BUILDFLAG(IS_MAC)
#include "net/ssl/ssl_platform_key_mac.h"
#endif  // BUILDFLAG(IS_WIN)

namespace client_certificates {

namespace {

// base::Callback requires a static initializer, so wrap it in this function and
// use a static base::NoDestructor to avoid creating a global static
// initializer.
internal::ConvertKeyCallback* GetConvertKeyCallback() {
  static base::NoDestructor<internal::ConvertKeyCallback> s_convert_key;
  return s_convert_key.get();
}

}  // namespace

scoped_refptr<net::SSLPrivateKey> SSLPrivateKeyFromUnexportableSigningKeySlowly(
    const crypto::UnexportableSigningKey& key) {
  if (!GetConvertKeyCallback()->is_null()) {
    return GetConvertKeyCallback()->Run(key);
  }
#if BUILDFLAG(IS_WIN)
  return net::WrapUnexportableKeySlowly(key);
#elif BUILDFLAG(IS_MAC)
  return net::WrapUnexportableKey(key);
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN)
}

namespace internal {

void SetConverterForTesting(internal::ConvertKeyCallback callback) {
  // At least one of the two needs to be null, as nesting of scoped converters
  // is not supported.
  CHECK(GetConvertKeyCallback()->is_null() || !callback);
  *GetConvertKeyCallback() = callback;
}

}  // namespace internal
}  // namespace client_certificates
