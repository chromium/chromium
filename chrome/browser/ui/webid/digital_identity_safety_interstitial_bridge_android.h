// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_UI_WEBID_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_BRIDGE_ANDROID_H_

#include <jni.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/content_browser_client.h"

namespace content {
class WebContents;
}

namespace url {
class Origin;
}

// The C++ counterpart to DigitalIdentitySafetyInterstitialBridge.java
// Initiates showing modal dialog asking user whether they want to
// share their identity with website.
class DigitalIdentitySafetyInterstitialBridgeAndroid {
 public:
  DigitalIdentitySafetyInterstitialBridgeAndroid();
  virtual ~DigitalIdentitySafetyInterstitialBridgeAndroid();

  DigitalIdentitySafetyInterstitialBridgeAndroid(
      const DigitalIdentitySafetyInterstitialBridgeAndroid&) = delete;
  DigitalIdentitySafetyInterstitialBridgeAndroid& operator=(
      const DigitalIdentitySafetyInterstitialBridgeAndroid&) = delete;

  content::ContentBrowserClient::DigitalIdentityInterstitialAbortCallback
  ShowInterstitialIfNeeded(
      content::WebContents& web_contents,
      const url::Origin& origin,
      bool is_only_requesting_age,
      content::ContentBrowserClient::DigitalIdentityInterstitialCallback
          callback);

  void OnInterstitialDone(JNIEnv* env, jint status_for_metrics);

 private:
  void Abort();

  base::android::ScopedJavaGlobalRef<jobject> j_bridge_;

  content::ContentBrowserClient::DigitalIdentityInterstitialCallback callback_;

  base::WeakPtrFactory<DigitalIdentitySafetyInterstitialBridgeAndroid>
      weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_WEBID_DIGITAL_IDENTITY_SAFETY_INTERSTITIAL_BRIDGE_ANDROID_H_
