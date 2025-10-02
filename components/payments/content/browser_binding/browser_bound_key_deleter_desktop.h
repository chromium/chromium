// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_DESKTOP_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_DESKTOP_H_

#include "components/payments/content/browser_binding/browser_bound_key_deleter.h"

namespace payments {

class BrowserBoundKeyDeleterDesktop : public BrowserBoundKeyDeleter {
 public:
  explicit BrowserBoundKeyDeleterDesktop();

  ~BrowserBoundKeyDeleterDesktop() override;

  void RemoveInvalidBBKs() override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_DESKTOP_H_
