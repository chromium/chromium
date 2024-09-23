// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SCOPED_SSL_KEY_CONVERTER_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SCOPED_SSL_KEY_CONVERTER_H_

#include <optional>

#include "crypto/scoped_mock_unexportable_key_provider.h"

namespace client_certificates {

// Testing utility which will mock both the UnexportableKeyProvider as well
// as the SSLKeyConverter in a way where they can work together.
class ScopedSSLKeyConverter {
 public:
  // This class will also mock crypto::UnexportableSigningKey providers
  // based on the value of `supports_unexportable`.
  explicit ScopedSSLKeyConverter(bool supports_unexportable = true);
  ~ScopedSSLKeyConverter();

 private:
  const bool supports_unexportable_;
  std::optional<crypto::ScopedMockUnexportableKeyProvider>
      unexportable_provider_;
  std::optional<crypto::ScopedNullUnexportableKeyProvider>
      unexportable_null_provider_;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SCOPED_SSL_KEY_CONVERTER_H_
