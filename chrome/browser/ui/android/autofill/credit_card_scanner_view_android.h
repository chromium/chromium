// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_CREDIT_CARD_SCANNER_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_CREDIT_CARD_SCANNER_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_view.h"

namespace content {
class WebContents;
}

namespace autofill {

class CreditCardScannerViewDelegate;

// Android implementation of the view for credit card scanner UI. Uses Android
// APIs through JNI service.
class CreditCardScannerViewAndroid : public CreditCardScannerView {
 public:
  CreditCardScannerViewAndroid(
      const base::WeakPtr<CreditCardScannerViewDelegate>& delegate,
      content::WebContents* web_contents);

  CreditCardScannerViewAndroid(const CreditCardScannerViewAndroid&) = delete;
  CreditCardScannerViewAndroid& operator=(const CreditCardScannerViewAndroid&) =
      delete;

  ~CreditCardScannerViewAndroid() override;

  // Called by JNI when user cancelled credit card scan.
  void ScanCancelled(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& object);

  // Called by JNI when credit card scan completed successfully.
  void ScanCompleted(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& object,
                     const std::u16string& card_holder_name,
                     const std::u16string& card_number,
                     jint expiration_month,
                     jint expiration_year);

 private:
  // CreditCardScannerView implementation.
  void Show() override;

  // The object to be notified when scanning was cancelled or completed.
  base::WeakPtr<CreditCardScannerViewDelegate> delegate_;

  // The corresponding Java object that uses Android APIs for scanning.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_CREDIT_CARD_SCANNER_VIEW_ANDROID_H_
