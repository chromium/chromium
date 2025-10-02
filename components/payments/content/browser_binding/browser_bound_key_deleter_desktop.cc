// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_deleter_desktop.h"

#include <memory>

#include "components/payments/content/browser_binding/browser_bound_key_deleter.h"

namespace payments {

std::unique_ptr<BrowserBoundKeyDeleter> GetBrowserBoundKeyDeleterInstance(
    scoped_refptr<WebPaymentsWebDataService> web_data_service) {
  return std::make_unique<BrowserBoundKeyDeleterDesktop>();
}

BrowserBoundKeyDeleterDesktop::BrowserBoundKeyDeleterDesktop() = default;

BrowserBoundKeyDeleterDesktop::~BrowserBoundKeyDeleterDesktop() = default;

void BrowserBoundKeyDeleterDesktop::RemoveInvalidBBKs() {
  // TODO(crbug.com/441553248): Implement in a follow-up CL.
}

}  // namespace payments
