// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_H_

#include <cstdint>
#include <vector>

#include "components/payments/content/browser_binding/browser_bound_key.h"

namespace payments {

// A fake used in tests to provide instances of BrowserBoundKey.
class FakeBrowserBoundKey : public BrowserBoundKey {
 public:
  // Constructs a fake browser bound key that returns `identifier`,
  // `public_key_as_cose_key`, and `signature` from the respective calls.
  // `algorithm_identifier` is the COSE Algorithm identifier that
  // `FakeBrowserBoundKeyStore::GetOrCreateBrowserBoundKeyForCredentialId()`
  // will match. `Sign()` will compare its input, against
  // `expected_client_data`, returning an empty signature when these do not
  // match.
  // Set `is_new` to false when the key is expected to be retrieved even when
  // its algorithm is not listed.
  FakeBrowserBoundKey(std::vector<uint8_t> identifier,
                      std::vector<uint8_t> public_key_as_cose_key,
                      std::vector<uint8_t> signature,
                      int32_t algorithm_identifier,
                      std::vector<uint8_t> expected_client_data,
                      bool is_new = true);
  FakeBrowserBoundKey(const FakeBrowserBoundKey& other);
  FakeBrowserBoundKey& operator=(const FakeBrowserBoundKey& other);
  ~FakeBrowserBoundKey() override;

  std::vector<uint8_t> GetIdentifier() const override;
  std::vector<uint8_t> Sign(const std::vector<uint8_t>& client_data) override;
  std::vector<uint8_t> GetPublicKeyAsCoseKey() const override;

  int32_t algorithm_identifier() const { return algorithm_identifier_; }
  bool is_new() const { return is_new_; }

 private:
  std::vector<uint8_t> identifier_;
  std::vector<uint8_t> public_key_as_cose_key_;
  std::vector<uint8_t> signature_;
  int32_t algorithm_identifier_;
  std::vector<uint8_t> expected_client_data_;
  bool is_new_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_H_
