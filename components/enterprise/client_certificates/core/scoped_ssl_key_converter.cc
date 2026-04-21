// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/scoped_ssl_key_converter.h"

#include "base/notreached.h"
#include "components/enterprise/client_certificates/core/ssl_key_converter.h"
#include "crypto/keypair.h"
#include "net/ssl/crypto_private_key.h"
#include "net/ssl/ssl_private_key.h"

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
  auto private_key =
      crypto::keypair::PrivateKey::FromEcP256PrivateKey(key.GetWrappedKey());
  if (!private_key) {
    return nullptr;
  }

  return net::WrapCryptoPrivateKey(std::move(*private_key));
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
