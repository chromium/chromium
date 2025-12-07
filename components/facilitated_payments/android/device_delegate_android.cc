// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/device_delegate_android.h"

#include <memory>
#include <string_view>

#include "base/android/application_status_listener.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/facilitated_payments/android/facilitated_payments_app_info_list_android.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/validation/payment_link_validator.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/facilitated_payments/android/java/jni_headers/DeviceDelegate_jni.h"

namespace payments::facilitated {

DeviceDelegateAndroid::DeviceDelegateAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {}

DeviceDelegateAndroid::~DeviceDelegateAndroid() = default;

WalletEligibilityForPixAccountLinking
DeviceDelegateAndroid::IsPixAccountLinkingSupported() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  jint eligibility =
      Java_DeviceDelegate_getWalletEligibilityForPixAccountLinking(env);
  CHECK(eligibility >= static_cast<jint>(
                           WalletEligibilityForPixAccountLinking::kEligible) &&
        eligibility <= static_cast<jint>(WalletEligibilityForPixAccountLinking::
                                             kWalletVersionNotSupported));
  return static_cast<WalletEligibilityForPixAccountLinking>(eligibility);
}

void DeviceDelegateAndroid::LaunchPixAccountLinkingPage(std::string email) {
  if (!web_contents_ || !web_contents_->GetNativeView() ||
      !web_contents_->GetNativeView()->GetWindowAndroid()) {
    // TODO(crbug.com/419108993): Log metrics.
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DeviceDelegate_openPixAccountLinkingPageInWallet(
      env, web_contents_->GetTopLevelNativeWindow()->GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, email));
}

void DeviceDelegateAndroid::SetOnReturnToChromeCallbackAndObserveAppState(
    base::OnceClosure callback) {
  on_return_to_chrome_callback_ = std::move(callback);
  // It's possible that Chrome is already in the background when the app status
  // listener is initialized.
  is_chrome_in_background_ =
      base::android::ApplicationStatusListener::GetState() ==
      base::android::ApplicationState::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES;
  app_status_listener_ = base::android::ApplicationStatusListener::New(
      base::BindRepeating(&DeviceDelegateAndroid::OnApplicationStateChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DeviceDelegateAndroid::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  if (on_application_state_changed_callback_for_testing_) {
    std::move(on_application_state_changed_callback_for_testing_).Run();
  }

  // The observer is initialized only after setting the callback, and is reset
  // after the callback is run.
  CHECK(on_return_to_chrome_callback_);

  if (state ==
      base::android::ApplicationState::
          APPLICATION_STATE_HAS_STOPPED_ACTIVITIES) {  // Chrome to background.
    is_chrome_in_background_ = true;
  } else if (state ==
             base::android::ApplicationState::
                 APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {  // Chrome to
                                                              // foreground.
    if (!is_chrome_in_background_) {
      return;
    }
    std::move(on_return_to_chrome_callback_).Run();

    // No need to observe until the next Pix account linking flow.
    app_status_listener_.reset();
  }
}

std::unique_ptr<FacilitatedPaymentsAppInfoList>
DeviceDelegateAndroid::GetSupportedPaymentApps(const GURL& payment_link_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> raw_array =
      Java_DeviceDelegate_getSupportedPaymentApps(
          env, url::GURLAndroid::FromNativeGURL(env, payment_link_url),
          web_contents_->GetTopLevelNativeWindow()->GetJavaObject());
  return std::make_unique<FacilitatedPaymentsAppInfoListAndroid>(
      std::move(raw_array));
}

bool DeviceDelegateAndroid::InvokePaymentApp(std::string_view package_name,
                                             std::string_view activity_name,
                                             const GURL& payment_link_url) {
  PaymentLinkValidator validator;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DeviceDelegate_invokePaymentApp(
      env, base::android::ConvertUTF8ToJavaString(env, package_name),
      base::android::ConvertUTF8ToJavaString(env, activity_name),
      base::android::ConvertUTF8ToJavaString(
          env, SchemeToString(validator.GetScheme(payment_link_url))),
      url::GURLAndroid::FromNativeGURL(env, payment_link_url),
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject());
}

bool DeviceDelegateAndroid::IsPixSupportAvailableViaGboard() const {
  if (!web_contents_ || !web_contents_->GetNativeView() ||
      !web_contents_->GetNativeView()->GetWindowAndroid()) {
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DeviceDelegate_isPixSupportAvailableViaGboard(
      env, web_contents_->GetTopLevelNativeWindow()->GetJavaObject());
}

}  // namespace payments::facilitated

DEFINE_JNI(DeviceDelegate)
