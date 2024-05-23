// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_ENCRYPTION_VERIFICATION_H_
#define COMPONENTS_REPORTING_ENCRYPTION_VERIFICATION_H_

#include <string>
#include <string_view>

#include "components/reporting/util/status.h"

namespace reporting {

// Helper class that verifies an Ed25519 signed message received from
// the server. It uses boringssl implementation available on the client.
class SignatureVerifier {
 public:
  // Well-known public signature verification keys that is used to verify
  // that signed data is indeed originating from reporting server.
  // Exists in two flavors: PROD and DEV.
  static std::string_view VerificationKey();
  static std::string_view VerificationKeyDev();

  // Ed25519 |verification_public_key| must consist of kKeySize bytes.
  explicit SignatureVerifier(std::string_view verification_public_key);

  // Actual verification - returns error status if provided |signature| does not
  // match |message|. Signature must be kSignatureSize bytes.
  Status Verify(std::string_view message, std::string_view signature);

 private:
  std::string verification_public_key_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_ENCRYPTION_VERIFICATION_H_
