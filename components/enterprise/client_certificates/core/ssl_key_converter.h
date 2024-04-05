// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_KEY_CONVERTER_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_KEY_CONVERTER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "crypto/ec_private_key.h"
#include "crypto/unexportable_key.h"

namespace net {
class SSLPrivateKey;
}  // namespace net

namespace client_certificates {

// Class used to convert keys from different format into SSLPrivateKeys. Make
// sure that a given key will only be used to sign TLS payloads to avoid
// cross-protocol attacks.
class SSLKeyConverter {
 public:
  virtual ~SSLKeyConverter();

  static std::unique_ptr<SSLKeyConverter> Get();

  // Returns an SSLPrivateKey from the given unexportable `key`. The function is
  // marked as slow since, depending on the platform, the conversion may involve
  // a round-trip to a hardware security module (e.g. TPM).
  virtual scoped_refptr<net::SSLPrivateKey> ConvertUnexportableKeySlowly(
      const crypto::UnexportableSigningKey& key) = 0;

  // Returns an SSLPrivateKey from the given `key`. Since `crypto::ECPrivateKey`
  // wraps an OpenSSL private key, this conversion is purely arithmetical.
  virtual scoped_refptr<net::SSLPrivateKey> ConvertECKey(
      const crypto::ECPrivateKey& key) = 0;
};

namespace internal {

void SetConverterForTesting(std::unique_ptr<SSLKeyConverter> (*func)());

}  // namespace internal
}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_KEY_CONVERTER_H_
