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
  // Constructs a fake browser bound key that returns `public_key_as_cose_key`,
  // and `signature` from the respective calls. `Sign()` will compare its input,
  // against `expected_client_data`, returning an empty signature when these do
  // not match.
  FakeBrowserBoundKey(std::vector<uint8_t> public_key_as_cose_key,
                      std::vector<uint8_t> signature,
                      std::vector<uint8_t> expected_client_data);
  FakeBrowserBoundKey(const FakeBrowserBoundKey& other);
  FakeBrowserBoundKey& operator=(const FakeBrowserBoundKey& other);
  ~FakeBrowserBoundKey() override;

  std::vector<uint8_t> Sign(const std::vector<uint8_t>& client_data) override;
  std::vector<uint8_t> GetPublicKeyAsCoseKey() override;

 private:
  std::vector<uint8_t> public_key_as_cose_key_;
  std::vector<uint8_t> signature_;
  std::vector<uint8_t> expected_client_data_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_FAKE_BROWSER_BOUND_KEY_H_
