// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_PASSKEY_BROWSER_BINDER_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_PASSKEY_BROWSER_BINDER_H_

#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

class MockPasskeyBrowserBinder : public PasskeyBrowserBinder {
 public:
  explicit MockPasskeyBrowserBinder(
      scoped_refptr<BrowserBoundKeyStore> key_store,
      scoped_refptr<WebPaymentsWebDataService> web_data_service);

  ~MockPasskeyBrowserBinder() override;

  MOCK_METHOD(void,
              GetAllBrowserBoundKeys,
              (base::OnceCallback<void(std::vector<BrowserBoundKeyMetadata>)>),
              (override));
  MOCK_METHOD(void,
              DeleteBrowserBoundKeys,
              (base::OnceClosure, std::vector<BrowserBoundKeyMetadata>),
              (override));
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_PASSKEY_BROWSER_BINDER_H_
