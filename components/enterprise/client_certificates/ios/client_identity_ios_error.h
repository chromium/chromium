// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CLIENT_IDENTITY_IOS_ERROR_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CLIENT_IDENTITY_IOS_ERROR_H_

#include <string_view>

namespace client_certificates {

// Represents the various error types that can occur during the instantiation
// of a `ClientIdentityIOS` instance.
// TODO(b/481664880): Add logging to UMA.
enum class ClientIdentityIOSError {
  kInvalidBaseIdentity = 0,
  kCertificateConversionFailed = 1,
  kPrivateKeyConversionFailed = 2,
  kSecIdentityCreateFailed = 3,
  kMaxValue = kSecIdentityCreateFailed
};

// Returns a string representation of `error`.
std::string_view ClientIdentityIOSErrorToString(ClientIdentityIOSError error);

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CLIENT_IDENTITY_IOS_ERROR_H_
