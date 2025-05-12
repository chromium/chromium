// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_H_

namespace payments::facilitated {

// A cross-platform interface that manages the Pix account linking flow. It is
// owned by `FacilitatedPaymentsClient`. There is 1 instance of this class per
// tab. Its lifecycle is same as that of `FacilitatedPaymentsClient`.

// The Pix account linking prompt is shown after the user has paid on their bank
// app and returned to Chrome. Some merchants show the order status causing page
// navigations. To overcome such cases, the manager should be associated with
// the tab, and not a single frame.
class PixAccountLinkingManager {
 public:
  virtual ~PixAccountLinkingManager() = default;

  // Initialize the Pix account linking flow. Virtual so it can be overridden in
  // tests.
  virtual void MaybeShowPixAccountLinkingPrompt();
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_H_
