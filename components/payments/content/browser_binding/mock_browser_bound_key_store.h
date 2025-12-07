// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEY_STORE_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEY_STORE_H_

#include <stdint.h>

#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

class MockBrowserBoundKeyStore : public BrowserBoundKeyStore {
 public:
  MockBrowserBoundKeyStore();

  MOCK_METHOD(std::unique_ptr<BrowserBoundKey>,
              GetOrCreateBrowserBoundKeyForCredentialId,
              (const std::vector<uint8_t>&, const CredentialInfoList&),
              (override));
  MOCK_METHOD(void, DeleteBrowserBoundKey, (std::vector<uint8_t>), (override));
  MOCK_METHOD(bool, GetDeviceSupportsHardwareKeys, (), (override));

 protected:
  ~MockBrowserBoundKeyStore() override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEY_STORE_H_
