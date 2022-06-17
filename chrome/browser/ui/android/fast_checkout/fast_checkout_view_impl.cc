// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/fast_checkout/fast_checkout_view_impl.h"

#include <memory>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

FastCheckoutViewImpl::FastCheckoutViewImpl(
    base::WeakPtr<FastCheckoutController> controller)
    : controller_(controller) {}

FastCheckoutViewImpl::~FastCheckoutViewImpl() = default;

void FastCheckoutViewImpl::OnOptionsSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& autofill_profile_java,
    const base::android::JavaParamRef<jobject>& credit_card_java) {}

void FastCheckoutViewImpl::OnDismiss(JNIEnv* env) {}

void FastCheckoutViewImpl::Show(
    base::span<const autofill::AutofillProfile> autofill_profiles,
    base::span<const autofill::CreditCard> credit_cards) {}

bool FastCheckoutViewImpl::RecreateJavaObject() {
  return false;
}

// static
std::unique_ptr<FastCheckoutView> FastCheckoutView::Create(
    base::WeakPtr<FastCheckoutController> controller) {
  return std::make_unique<FastCheckoutViewImpl>(controller);
}
