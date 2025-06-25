// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_DEVICE_DELEGATE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_DEVICE_DELEGATE_H_

#include "base/functional/callback.h"

namespace payments::facilitated {

// Abstract base class for device-specific facilitated payments operations.
// This class defines the interface for operations that require interaction
// with the underlying device or platform, such as checking and opening other
// applications.

// It is owned by FacilitatedPaymentsClient, and has the same lifecycle.
class DeviceDelegate {
 public:
  virtual ~DeviceDelegate() = default;

  // Returns true if Pix account linking is supported by the device.
  virtual bool IsPixAccountLinkingSupported() const = 0;

  // Takes user to the Pix account linking page.
  virtual void LaunchPixAccountLinkingPage() = 0;

  // Saves the `callback` to be run after the user leaves and then returns to
  // Chrome.
  virtual void SetOnReturnToChromeCallback(base::OnceClosure callback) = 0;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_DEVICE_DELEGATE_H_
