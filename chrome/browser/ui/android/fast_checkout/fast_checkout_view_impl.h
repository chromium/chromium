// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_FAST_CHECKOUT_FAST_CHECKOUT_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_ANDROID_FAST_CHECKOUT_FAST_CHECKOUT_VIEW_IMPL_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_view.h"

class FastCheckoutController;

// This class provides an Android implementation of the `FastCheckoutView`
// interface. It communicates with its `FastCheckoutBridge` Java counterpart via
// JNI.
class FastCheckoutViewImpl : public FastCheckoutView {
 public:
  explicit FastCheckoutViewImpl(
      base::WeakPtr<FastCheckoutController> controller);
  ~FastCheckoutViewImpl() override;

  void OnOptionsSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& autofill_profile_java,
      const base::android::JavaParamRef<jobject>& credit_card_java);
  void OnDismiss(JNIEnv* env);
  void OpenAutofillProfileSettings(JNIEnv* env);
  void OpenCreditCardSettings(JNIEnv* env);

  // FastCheckoutView:
  void Show(
      const std::vector<const autofill::AutofillProfile*>& autofill_profiles,
      const std::vector<autofill::CreditCard*>& credit_cards) override;

 private:
  // Returns either true if the java counterpart of this bridge is initialized
  // successfully or false if the creation failed. This method  will create
  // the java object whenever Show() is called and re-use the same component if
  // already exist.
  bool RecreateJavaObjectIfNecessary();

  const base::WeakPtr<FastCheckoutController> controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_FAST_CHECKOUT_FAST_CHECKOUT_VIEW_IMPL_H_
