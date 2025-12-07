// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"

#include "base/notreached.h"
#include "components/enterprise/client_certificates/core/ssl_key_converter.h"
#include "net/ssl/openssl_private_key.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace client_certificates {

namespace {

scoped_refptr<net::SSLPrivateKey> ConvertKey(
    bool supports_unexportable,
    const crypto::UnexportableSigningKey& key) {
  if (!supports_unexportable) {
    // If unexportable keys are not supported, then there shouldn't be an
    // unexportable key instance to convert.
    NOTREACHED();
  }

  // In this case, the underlying unexportable key is effectively a stub
  // representing an EC private key generated via BoringSSL. The wrapped value
  // represents a DER-encoded ECPrivateKey structure.
  auto wrapped = key.GetWrappedKey();

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  CBS cbs;
  CBS_init(&cbs, wrapped.data(), wrapped.size());
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_parse_private_key(&cbs, p256.get()));
  if (!ec_key || CBS_len(&cbs) != 0) {
    return nullptr;
  }

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get())) {
    return nullptr;
  }

  return net::WrapOpenSSLPrivateKey(std::move(pkey));
}

}  // namespace

ScopedSSLKeyConverter::ScopedSSLKeyConverter(bool supports_unexportable) {
  if (supports_unexportable) {
    unexportable_provider_.emplace();
    internal::SetConverterForTesting(base::BindRepeating(&ConvertKey, true));
  } else {
    unexportable_null_provider_.emplace();
    internal::SetConverterForTesting(base::BindRepeating(&ConvertKey, false));
  }
}

ScopedSSLKeyConverter::~ScopedSSLKeyConverter() {
  internal::SetConverterForTesting({});
}

}  // namespace client_certificates
