// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ssl_key_converter.h"

#include "base/check.h"
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
std::unique_ptr<SSLKeyConverter> (*g_mock_converter)() = nullptr;
}  // namespace

class SSLKeyConverterImpl : public SSLKeyConverter {
 public:
  SSLKeyConverterImpl();
  ~SSLKeyConverterImpl() override;

  // SSLKeyConverter:
  scoped_refptr<net::SSLPrivateKey> ConvertUnexportableKeySlowly(
      const crypto::UnexportableSigningKey& key) override;
  scoped_refptr<net::SSLPrivateKey> ConvertECKey(
      const crypto::ECPrivateKey& key) override;
};

SSLKeyConverter::~SSLKeyConverter() = default;

std::unique_ptr<SSLKeyConverter> SSLKeyConverter::Get() {
  if (g_mock_converter) {
    return g_mock_converter();
  }
  return std::make_unique<SSLKeyConverterImpl>();
}

SSLKeyConverterImpl::SSLKeyConverterImpl() = default;
SSLKeyConverterImpl::~SSLKeyConverterImpl() = default;

scoped_refptr<net::SSLPrivateKey>
SSLKeyConverterImpl::ConvertUnexportableKeySlowly(
    const crypto::UnexportableSigningKey& key) {
#if BUILDFLAG(IS_WIN)
  return net::WrapUnexportableKeySlowly(key);
#elif BUILDFLAG(IS_MAC)
  return net::WrapUnexportableKey(key);
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN)
}

scoped_refptr<net::SSLPrivateKey> SSLKeyConverterImpl::ConvertECKey(
    const crypto::ECPrivateKey& key) {
  return net::WrapOpenSSLPrivateKey(bssl::UpRef(key.key()));
}

namespace internal {

void SetConverterForTesting(std::unique_ptr<SSLKeyConverter> (*func)()) {
  // At least one of the two needs to be null, as nesting of scoped converters
  // is not supported.
  CHECK(!g_mock_converter || !func);
  g_mock_converter = func;
}

}  // namespace internal
}  // namespace client_certificates
