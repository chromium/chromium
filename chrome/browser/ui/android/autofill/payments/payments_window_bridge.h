// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace autofill::payments {

class PaymentsWindowDelegate;

// C++ counterpart to PaymentsWindowBridge.java. Enables C++ feature code to
// open/close the ephemeral tab with PaymentsWindowCoordinator.java.
class PaymentsWindowBridge {
 public:
  explicit PaymentsWindowBridge(
      PaymentsWindowDelegate* payments_window_delegate);

  PaymentsWindowBridge(const PaymentsWindowBridge&) = delete;
  PaymentsWindowBridge& operator=(const PaymentsWindowBridge&) = delete;

  virtual ~PaymentsWindowBridge();

  // Opens an ephemeral tab with the given `url` and `title`, using the provided
  // `merchant_web_contents` associated with the merchant.
  virtual void OpenEphemeralTab(const GURL& url,
                                const std::u16string& title,
                                content::WebContents& merchant_web_contents);

  // Closes the ephemeral tab.
  virtual void CloseEphemeralTab();

  // Called when the ephemeral tab has finished a navigation.
  void OnNavigationFinished(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& clicked_url_object);

  // Called when observation has started for the WebContents.
  void OnWebContentsObservationStarted(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  // Called when WebContents is being destroyed.
  void OnWebContentsDestroyed(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_payments_window_bridge_;
  const raw_ref<PaymentsWindowDelegate> payments_window_delegate_;
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_BRIDGE_H_
