// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_DEVICE_DELEGATE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_DEVICE_DELEGATE_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "url/gurl.h"

namespace payments::facilitated {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.facilitated_payments
enum class WalletEligibilityForPixAccountLinking {
  kEligible = 0,
  kWalletNotInstalled = 1,
  kWalletVersionNotSupported = 2
};

// Abstract base class for device-specific facilitated payments operations.
// This class defines the interface for operations that require interaction
// with the underlying device or platform, such as checking and opening other
// applications.

// It is owned by FacilitatedPaymentsClient, and has the same lifecycle.
class DeviceDelegate {
 public:
  virtual ~DeviceDelegate() = default;

  // Returns true if Pix account linking is supported by the device.
  virtual WalletEligibilityForPixAccountLinking IsPixAccountLinkingSupported()
      const = 0;

  // Takes user to the Pix account linking page. The `email` is used to provide
  // the gaia account that the user is signed into.
  virtual void LaunchPixAccountLinkingPage(std::string email) = 0;

  // Observes the Chrome app, and runs the `callback` when the user returns to
  // Chrome.
  virtual void SetOnReturnToChromeCallbackAndObserveAppState(
      base::OnceClosure callback) = 0;

  virtual std::unique_ptr<FacilitatedPaymentsAppInfoList>
  GetSupportedPaymentApps(const GURL& payment_link_url) = 0;

  // Invokes a payment app with the given `package_name`, `activity_name`, and
  // `payment_link_url`. Returns `true` if the app was invoked successfully.
  virtual bool InvokePaymentApp(std::string_view package_name,
                                std::string_view activity_name,
                                const GURL& payment_link_url) = 0;

  // Returns true if Pix is supported via Gboard.
  virtual bool IsPixSupportAvailableViaGboard() const = 0;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_DEVICE_DELEGATE_H_
