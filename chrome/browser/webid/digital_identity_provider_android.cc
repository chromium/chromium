// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/digital_identity_provider_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/webid/jni_headers/DigitalIdentityProvider_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;

DigitalIdentityProviderAndroid::DigitalIdentityProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  j_digital_identity_provider_android_.Reset(
      Java_DigitalIdentityProvider_create(env,
                                            reinterpret_cast<intptr_t>(this)));
}

DigitalIdentityProviderAndroid::~DigitalIdentityProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_DigitalIdentityProvider_destroy(
      env, j_digital_identity_provider_android_);
}

void DigitalIdentityProviderAndroid::Request(content::WebContents* web_contents,
                                             const url::Origin& origin,
                                             const std::string& request,
                                             DigitalIdentityCallback callback) {
  callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_origin =
      ConvertUTF8ToJavaString(env, origin.Serialize());
  ScopedJavaLocalRef<jstring> j_request = ConvertUTF8ToJavaString(env, request);

  base::android::ScopedJavaLocalRef<jobject> j_window = nullptr;

  if (web_contents && web_contents->GetTopLevelNativeWindow()) {
    j_window = web_contents->GetTopLevelNativeWindow()->GetJavaObject();
  }

  Java_DigitalIdentityProvider_request(
      env, j_digital_identity_provider_android_, j_window, j_origin,
      j_request);
}

void DigitalIdentityProviderAndroid::OnReceive(JNIEnv* env,
                                               jstring j_digital_identity,
                                               jint j_status_for_metrics) {
  if (callback_) {
    std::string digital_identity =
        ConvertJavaStringToUTF8(env, j_digital_identity);

    auto status_for_metrics =
        static_cast<RequestStatusForMetrics>(j_status_for_metrics);
    std::move(callback_).Run(
        (status_for_metrics == RequestStatusForMetrics::kSuccess)
            ? base::expected<std::string, RequestStatusForMetrics>(
                  digital_identity)
            : base::unexpected(status_for_metrics));
  }
}
