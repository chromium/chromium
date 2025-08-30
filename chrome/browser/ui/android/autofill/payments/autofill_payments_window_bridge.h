// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_AUTOFILL_PAYMENTS_WINDOW_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_AUTOFILL_PAYMENTS_WINDOW_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace autofill::payments {

class AutofillPaymentsWindowDelegate;

// TODO(crbug.com/430575808): Remove `Autofill` prefix from
// `AutofillPaymentsWindowBridge` and `AutofillPaymentsWindowDelegate`.
// C++ counterpart to PaymentsWindowBridge.java. Enables C++ feature code to
// open/close the ephemeral tab with PaymentsWindowCoordinator.java.
class AutofillPaymentsWindowBridge {
 public:
  AutofillPaymentsWindowBridge(
      content::WebContents& contents,
      AutofillPaymentsWindowDelegate* autofill_payments_window_delegate);

  AutofillPaymentsWindowBridge(const AutofillPaymentsWindowBridge&) = delete;
  AutofillPaymentsWindowBridge& operator=(const AutofillPaymentsWindowBridge&) =
      delete;

  virtual ~AutofillPaymentsWindowBridge();

  // Opens an ephemeral tab with the given `url` and `title`.
  virtual void OpenEphemeralTab(const GURL& url, const std::u16string& title);

  // Closes the ephemeral tab.
  virtual void CloseEphemeralTab();

  // Called when the ephemeral tab has finished a navigation.
  void OnNavigationFinished(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& clicked_url_object);

  // Called when WebContents is being destroyed.
  void OnWebContentsDestroyed(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_payments_window_bridge_;
  const raw_ref<AutofillPaymentsWindowDelegate>
      autofill_payments_window_delegate_;
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_PAYMENTS_AUTOFILL_PAYMENTS_WINDOW_BRIDGE_H_
