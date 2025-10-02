// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_ANDROID_H_

#include "base/memory/scoped_refptr.h"
#include "components/payments/content/browser_binding/browser_bound_key_deleter.h"
#include "components/payments/content/web_payments_web_data_service.h"

namespace payments {

class BrowserBoundKeyDeleterAndroid : public BrowserBoundKeyDeleter {
 public:
  explicit BrowserBoundKeyDeleterAndroid(
      scoped_refptr<WebPaymentsWebDataService> web_data_service);

  ~BrowserBoundKeyDeleterAndroid() override;

  void RemoveInvalidBBKs() override;

 private:
  scoped_refptr<WebPaymentsWebDataService> web_data_service_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_ANDROID_H_
