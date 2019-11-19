// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/credit_card_scanner_view_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/chrome_jni_headers/CreditCardScannerBridge_jni.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_view_delegate.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;

namespace autofill {

// static
bool CreditCardScannerView::CanShow() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> java_object(
      Java_CreditCardScannerBridge_create(env, 0, nullptr));
  return Java_CreditCardScannerBridge_canScan(env, java_object);
}

// static
std::unique_ptr<CreditCardScannerView> CreditCardScannerView::Create(
    const base::WeakPtr<CreditCardScannerViewDelegate>& delegate,
    content::WebContents* web_contents) {
  return base::WrapUnique(
      new CreditCardScannerViewAndroid(delegate, web_contents));
}

CreditCardScannerViewAndroid::CreditCardScannerViewAndroid(
    const base::WeakPtr<CreditCardScannerViewDelegate>& delegate,
    content::WebContents* web_contents)
    : delegate_(delegate),
      java_object_(Java_CreditCardScannerBridge_create(
          base::android::AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this),
          web_contents->GetJavaWebContents())) {}

CreditCardScannerViewAndroid::~CreditCardScannerViewAndroid() {}

void CreditCardScannerViewAndroid::ScanCancelled(
    JNIEnv* env,
    const JavaParamRef<jobject>& object) {
  delegate_->ScanCancelled();
}

void CreditCardScannerViewAndroid::ScanCompleted(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jstring>& card_holder_name,
    const JavaParamRef<jstring>& card_number,
    jint expiration_month,
    jint expiration_year) {
  CreditCard card;
  card.SetNumber(base::android::ConvertJavaStringToUTF16(env, card_number));
  card.SetExpirationMonth(static_cast<int>(expiration_month));
  card.SetExpirationYear(static_cast<int>(expiration_year));

  delegate_->ScanCompleted(card);
}

void CreditCardScannerViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CreditCardScannerBridge_scan(env, java_object_);
}

}  // namespace autofill
