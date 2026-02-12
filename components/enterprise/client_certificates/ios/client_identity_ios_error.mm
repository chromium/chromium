// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/client_certificates/ios/client_identity_ios_error.h"

namespace client_certificates {

std::string_view ClientIdentityIOSErrorToString(ClientIdentityIOSError error) {
  switch (error) {
    case ClientIdentityIOSError::kInvalidBaseIdentity:
      return "Invalid base identity";
    case ClientIdentityIOSError::kCertificateConversionFailed:
      return "Certificate conversion failed";
    case ClientIdentityIOSError::kPrivateKeyConversionFailed:
      return "Private key conversion failed";
    case ClientIdentityIOSError::kSecIdentityCreateFailed:
      return "SecIdentityRef creation failed";
  }
}

}  // namespace client_certificates
