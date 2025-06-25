// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_

#include <memory>

#include "base/android/application_status_listener.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "content/public/browser/web_contents.h"

namespace payments::facilitated {

// Android implementation of `DeviceDelegate`.
class DeviceDelegateAndroid : public DeviceDelegate {
 public:
  explicit DeviceDelegateAndroid(content::WebContents* web_contents);
  DeviceDelegateAndroid(const DeviceDelegateAndroid&) = delete;
  DeviceDelegateAndroid& operator=(const DeviceDelegateAndroid&) = delete;
  ~DeviceDelegateAndroid() override;

  // Returns true if Google Wallet is installed, and its version supports Pix
  // account linking.
  bool IsPixAccountLinkingSupported() const override;

  // Opens the Pix account linking page in Google Wallet.
  void LaunchPixAccountLinkingPage() override;

  // The `callback` is called after the Chrome app goes to background and then
  // returns to the foreground. The `callback` is not called if the active tab
  // that called this method is closed or if the app itself is closed.
  void SetOnReturnToChromeCallback(base::OnceClosure callback) final;

 private:
  friend class DeviceDelegateAndroidTest;

  FRIEND_TEST_ALL_PREFIXES(DeviceDelegateAndroidTest,
                           ChromeGoesToBackgroundThenForeground_CallbackRun);
  FRIEND_TEST_ALL_PREFIXES(
      DeviceDelegateAndroidTest,
      ChromeGoesToForegroundWithoutGoingToBackground_CallbackNotRun);
  FRIEND_TEST_ALL_PREFIXES(DeviceDelegateAndroidTest,
                           ChromeGoesToBackground_CallbackNotRun);
  FRIEND_TEST_ALL_PREFIXES(
      DeviceDelegateAndroidTest,
      MultipleBackgroundForegroundCycles_CallbackRunOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(
      DeviceDelegateAndroidTest,
      CallbackSetAfterChromeAlreadyInBackground_ThenForeground_CallbackNotRun);

  // Called when the Chrome app's state changes.
  void OnApplicationStateChanged(base::android::ApplicationState state);

  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  // If true, the Chrome application is moved to the background.
  bool is_chrome_in_background_ = false;
  // Callback to be called when Chrome comes back to the foreground.
  base::OnceClosure on_return_to_chrome_callback_;

  base::WeakPtrFactory<DeviceDelegateAndroid> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_
