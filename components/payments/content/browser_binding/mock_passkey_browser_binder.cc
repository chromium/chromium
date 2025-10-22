// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/mock_passkey_browser_binder.h"

namespace payments {

MockPasskeyBrowserBinder::MockPasskeyBrowserBinder(
    scoped_refptr<BrowserBoundKeyStore> key_store,
    scoped_refptr<WebPaymentsWebDataService> web_data_service)
    : PasskeyBrowserBinder(key_store, web_data_service) {}

MockPasskeyBrowserBinder::~MockPasskeyBrowserBinder() = default;

}  // namespace payments
