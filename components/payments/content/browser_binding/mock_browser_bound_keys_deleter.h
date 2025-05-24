// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEYS_DELETER_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEYS_DELETER_H_

#include "components/payments/content/browser_binding/browser_bound_keys_deleter.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

class MockBrowserBoundKeyDeleter : public BrowserBoundKeyDeleter {
 public:
  MockBrowserBoundKeyDeleter();

  ~MockBrowserBoundKeyDeleter() override;

  MOCK_METHOD(void, RemoveInvalidBBKs, (), (override));
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_MOCK_BROWSER_BOUND_KEYS_DELETER_H_
