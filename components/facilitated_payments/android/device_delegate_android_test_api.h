// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_TEST_API_H_

#include "base/android/application_status_listener.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/facilitated_payments/android/device_delegate_android.h"

namespace payments::facilitated {

class DeviceDelegateAndroidTestApi {
 public:
  explicit DeviceDelegateAndroidTestApi(DeviceDelegateAndroid* delegate)
      : delegate_(CHECK_DEREF(delegate)) {}
  DeviceDelegateAndroidTestApi(const DeviceDelegateAndroidTestApi&) = delete;
  DeviceDelegateAndroidTestApi& operator=(const DeviceDelegateAndroidTestApi&) =
      delete;
  ~DeviceDelegateAndroidTestApi() = default;

  void SetOnApplicationStateChangedCallbackForTesting(
      base::OnceClosure callback) {
    delegate_->on_application_state_changed_callback_for_testing_ =
        std::move(callback);
  }

  base::android::ApplicationStatusListener* app_status_listener() {
    return delegate_->app_status_listener_.get();
  }

 private:
  const raw_ref<DeviceDelegateAndroid> delegate_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_TEST_API_H_
