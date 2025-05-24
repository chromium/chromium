// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/digital_credentials/digital_identity_safety_interstitial_bridge_android.h"

#include "base/android/jni_android.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/digital_credentials/jni_headers/DigitalIdentitySafetyInterstitialBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

DigitalIdentitySafetyInterstitialBridgeAndroid::
    DigitalIdentitySafetyInterstitialBridgeAndroid()
    : weak_ptr_factory_(this) {
  JNIEnv* env = AttachCurrentThread();
  j_bridge_ = Java_DigitalIdentitySafetyInterstitialBridge_create(
      env, reinterpret_cast<intptr_t>(this));
}

DigitalIdentitySafetyInterstitialBridgeAndroid::
    ~DigitalIdentitySafetyInterstitialBridgeAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_DigitalIdentitySafetyInterstitialBridge_destroy(env, j_bridge_);
}

content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback
DigitalIdentitySafetyInterstitialBridgeAndroid::ShowInterstitial(
    content::WebContents& web_contents,
    const url::Origin& origin,
    content::DigitalIdentityInterstitialType interstitial_type,
    content::DigitalIdentityProvider::DigitalIdentityInterstitialCallback
        callback) {
  callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_origin = origin.ToJavaObject(env);

  base::android::ScopedJavaLocalRef<jobject> j_window = nullptr;
  if (web_contents.GetTopLevelNativeWindow()) {
    j_window = web_contents.GetTopLevelNativeWindow()->GetJavaObject();
  }

  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();

  Java_DigitalIdentitySafetyInterstitialBridge_showInterstitial(
      env, j_bridge_, j_window, j_origin, static_cast<int>(interstitial_type));

  // If no interstitial was shown,
  // DigitalIdentitySafetyInterstitialBridgeAndroid will have been destroyed.
  return weak_ptr.get()
             ? base::BindOnce(
                   &DigitalIdentitySafetyInterstitialBridgeAndroid::Abort,
                   weak_ptr_factory_.GetWeakPtr())
             : base::OnceClosure();
}

void DigitalIdentitySafetyInterstitialBridgeAndroid::OnInterstitialDone(
    JNIEnv* env,
    jint status_for_metrics) {
  std::move(callback_).Run(
      static_cast<content::DigitalIdentityProvider::RequestStatusForMetrics>(
          status_for_metrics));
}

void DigitalIdentitySafetyInterstitialBridgeAndroid::Abort() {
  JNIEnv* env = AttachCurrentThread();
  Java_DigitalIdentitySafetyInterstitialBridge_abort(env, j_bridge_);
}
