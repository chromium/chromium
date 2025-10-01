// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/mock_browser_bound_keys_deleter.h"

namespace payments {

MockBrowserBoundKeyDeleter::MockBrowserBoundKeyDeleter()
    : BrowserBoundKeyDeleter(/*web_data_service=*/nullptr) {}

MockBrowserBoundKeyDeleter::~MockBrowserBoundKeyDeleter() = default;

}  // namespace payments
