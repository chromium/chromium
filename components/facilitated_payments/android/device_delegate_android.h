// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_

#include <memory>
#include <string_view>

#include "base/android/application_status_listener.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "content/public/browser/web_contents.h"

namespace payments::facilitated {

class DeviceDelegateAndroidTestApi;

// Android implementation of `DeviceDelegate`.
class DeviceDelegateAndroid : public DeviceDelegate {
 public:
  explicit DeviceDelegateAndroid(content::WebContents* web_contents);
  DeviceDelegateAndroid(const DeviceDelegateAndroid&) = delete;
  DeviceDelegateAndroid& operator=(const DeviceDelegateAndroid&) = delete;
  ~DeviceDelegateAndroid() override;

  // Returns eligible if Google Wallet is installed, and its version supports
  // Pix account linking.
  WalletEligibilityForPixAccountLinking IsPixAccountLinkingSupported()
      const override;

  // Opens the Pix account linking page in Google Wallet. The `email` is set to
  // the gaia account that the user logged into.
  void LaunchPixAccountLinkingPage(std::string email) override;

  // Starts observing the Chrome app status. Runs the `callback` if the Chrome
  // app is moved to the background and then to the foreground. Stops observing
  // after running the `callback`. The `callback` is not called if the active
  // tab that called this method is closed or if the Chrome app itself is
  // closed.
  void SetOnReturnToChromeCallbackAndObserveAppState(
      base::OnceClosure callback) final;

  std::unique_ptr<FacilitatedPaymentsAppInfoList> GetSupportedPaymentApps(
      const GURL& payment_link_url) override;

  bool InvokePaymentApp(std::string_view package_name,
                        std::string_view activity_name,
                        const GURL& payment_link_url) override;

  bool IsPixSupportAvailableViaGboard() const override;

 private:
  friend class DeviceDelegateAndroidTestApi;

  // Called when the Chrome app's state changes.
  void OnApplicationStateChanged(base::android::ApplicationState state);

  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  // If true, the Chrome application is moved to the background.
  bool is_chrome_in_background_ = false;
  // Callback to be called when Chrome comes back to the foreground.
  base::OnceClosure on_return_to_chrome_callback_;
  // A test-only callback that is run when `OnApplicationStateChanged` is
  // called.
  base::OnceClosure on_application_state_changed_callback_for_testing_;

  base::WeakPtrFactory<DeviceDelegateAndroid> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_
