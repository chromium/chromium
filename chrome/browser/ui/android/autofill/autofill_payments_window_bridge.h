// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_WINDOW_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_WINDOW_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// C++ counterpart to PaymentsWindowBridge.java. Enables C++ feature code to
// open/close the ephemeral tab with PaymentsWindowCoordinator.java.
class AutofillPaymentsWindowBridge {
 public:
  explicit AutofillPaymentsWindowBridge(content::WebContents& contents);

  AutofillPaymentsWindowBridge(const AutofillPaymentsWindowBridge&) = delete;
  AutofillPaymentsWindowBridge& operator=(const AutofillPaymentsWindowBridge&) =
      delete;

  virtual ~AutofillPaymentsWindowBridge();

  virtual void OpenEphemeralTab(const GURL& url, const std::u16string& title);

  virtual void CloseEphemeralTab();

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_payments_window_bridge_;
};
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_WINDOW_BRIDGE_H_
