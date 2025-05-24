// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEYS_DELETER_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEYS_DELETER_H_

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/payments/content/payment_manifest_web_data_service.h"

namespace payments {

class BrowserBoundKeyDeleter : public KeyedService {
 public:
  explicit BrowserBoundKeyDeleter(
      scoped_refptr<PaymentManifestWebDataService> web_data_service);

  // Non-copyable
  BrowserBoundKeyDeleter(const BrowserBoundKeyDeleter&) = delete;
  BrowserBoundKeyDeleter operator=(const BrowserBoundKeyDeleter&) = delete;

  // Non-moveable
  BrowserBoundKeyDeleter(const BrowserBoundKeyDeleter&&) = delete;
  BrowserBoundKeyDeleter operator=(const BrowserBoundKeyDeleter&&) = delete;

  ~BrowserBoundKeyDeleter() override;

  // Starts the asynchronous process to find browser bound keys and delete them.
  // Declared virtual to allow overriding by testing mocks.
  virtual void RemoveInvalidBBKs();

 private:
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEYS_DELETER_H_
