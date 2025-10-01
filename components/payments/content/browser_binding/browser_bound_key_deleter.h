// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/payments/content/web_payments_web_data_service.h"

namespace payments {

class BrowserBoundKeyDeleter : public KeyedService {
 public:
  BrowserBoundKeyDeleter() = default;

  // Non-copyable
  BrowserBoundKeyDeleter(const BrowserBoundKeyDeleter&) = delete;
  BrowserBoundKeyDeleter operator=(const BrowserBoundKeyDeleter&) = delete;

  // Non-moveable
  BrowserBoundKeyDeleter(const BrowserBoundKeyDeleter&&) = delete;
  BrowserBoundKeyDeleter operator=(const BrowserBoundKeyDeleter&&) = delete;

  ~BrowserBoundKeyDeleter() override = default;

  // Starts the asynchronous process to find browser bound keys and delete them.
  virtual void RemoveInvalidBBKs() = 0;
};

// Get a platform specific instance of the BrowserBoundKeyDeleter. This function
// has per-platform implementations.
std::unique_ptr<BrowserBoundKeyDeleter> GetBrowserBoundKeyDeleterInstance(
    scoped_refptr<WebPaymentsWebDataService> web_data_service);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_H_
