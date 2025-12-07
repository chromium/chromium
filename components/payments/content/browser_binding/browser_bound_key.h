// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_H_

#include <vector>
namespace payments {

// An interface representing a browser bound key that can be used to sign over
// clientDataJson.
class BrowserBoundKey {
 public:
  BrowserBoundKey() = default;
  BrowserBoundKey(const BrowserBoundKey&) = delete;
  BrowserBoundKey& operator=(const BrowserBoundKey&) = delete;
  virtual ~BrowserBoundKey() = default;

  // Returns the identifier of this browser bound key as a new vector.
  virtual std::vector<uint8_t> GetIdentifier() const = 0;

  // Signs the provided clientDataJson and return the signature.
  virtual std::vector<uint8_t> Sign(
      const std::vector<uint8_t>& client_data) = 0;

  // Returns the public key of this browser bound key encoded as a COSE_Key
  // including the algorithm type and parameters.
  //
  // See credentialPublicKey in
  // https://www.w3.org/TR/webauthn-2/#sctn-attested-credential-data.
  virtual std::vector<uint8_t> GetPublicKeyAsCoseKey() const = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_H_
