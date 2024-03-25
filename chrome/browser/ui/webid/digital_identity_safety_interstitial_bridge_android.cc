// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/digital_identity_safety_interstitial_bridge_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/ui/android/webid/jni_headers/DigitalIdentitySafetyInterstitialBridge_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

DigitalIdentitySafetyInterstitialBridgeAndroid::
    DigitalIdentitySafetyInterstitialBridgeAndroid() {
  JNIEnv* env = AttachCurrentThread();
  j_bridge_ = Java_DigitalIdentitySafetyInterstitialBridge_create(
      env, reinterpret_cast<intptr_t>(this));
}

DigitalIdentitySafetyInterstitialBridgeAndroid::
    ~DigitalIdentitySafetyInterstitialBridgeAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_DigitalIdentitySafetyInterstitialBridge_destroy(env, j_bridge_);
}

void DigitalIdentitySafetyInterstitialBridgeAndroid::ShowInterstitialIfNeeded(
    content::WebContents& web_contents,
    const url::Origin& origin,
    content::ContentBrowserClient::DigitalIdentityInterstitialCallback
        callback) {
  callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_origin = origin.ToJavaObject();

  base::android::ScopedJavaLocalRef<jobject> j_window = nullptr;
  if (web_contents.GetTopLevelNativeWindow()) {
    j_window = web_contents.GetTopLevelNativeWindow()->GetJavaObject();
  }

  Java_DigitalIdentitySafetyInterstitialBridge_showInterstitialIfNeeded(
      env, j_bridge_, j_window, j_origin);
}

void DigitalIdentitySafetyInterstitialBridgeAndroid::OnInterstitialDone(
    JNIEnv* env,
    jint status_for_metrics) {
  std::move(callback_).Run(
      static_cast<content::DigitalIdentityProvider::RequestStatusForMetrics>(
          status_for_metrics));
}
