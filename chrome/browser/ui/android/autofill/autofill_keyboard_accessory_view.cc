// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"

#include "chrome/android/features/keyboard_accessory/jni_headers/AutofillKeyboardAccessoryViewBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillKeyboardAccessoryView::AutofillKeyboardAccessoryView(
    AutofillPopupController* controller)
    : controller_(controller) {
  java_object_.Reset(Java_AutofillKeyboardAccessoryViewBridge_create(
      base::android::AttachCurrentThread()));
}

AutofillKeyboardAccessoryView::~AutofillKeyboardAccessoryView() {
  Java_AutofillKeyboardAccessoryViewBridge_resetNativeViewPointer(
      base::android::AttachCurrentThread(), java_object_);
}

bool AutofillKeyboardAccessoryView::Initialize() {
  ui::ViewAndroid* view_android = controller_->container_view();
  if (!view_android)
    return false;
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android)
    return false;  // The window might not be attached (yet or anymore).
  Java_AutofillKeyboardAccessoryViewBridge_init(
      base::android::AttachCurrentThread(), java_object_,
      reinterpret_cast<intptr_t>(this), window_android->GetJavaObject());
  return true;
}

void AutofillKeyboardAccessoryView::Hide() {
  Java_AutofillKeyboardAccessoryViewBridge_dismiss(
      base::android::AttachCurrentThread(), java_object_);
}

void AutofillKeyboardAccessoryView::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AutofillKeyboardAccessoryViewBridge_createAutofillSuggestionArray(
          env, controller_->GetLineCount());

  size_t position = 0;
  for (int i = 0; i < controller_->GetLineCount(); ++i) {
    const Suggestion& suggestion = controller_->GetSuggestionAt(i);
    int android_icon_id = 0;
    if (!suggestion.icon.empty()) {
      android_icon_id = ResourceMapper::MapToJavaDrawableId(
          GetIconResourceID(suggestion.icon));
    }
    // Set the offer title to display as the item tag.
    std::u16string item_tag = std::u16string();
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableOffersInClankKeyboardAccessory) &&
        !suggestion.offer_label.empty()) {
      item_tag = suggestion.offer_label;
      // If the offer label is not empty then replace the network icon by the
      // offer tag.
      android_icon_id =
          ResourceMapper::MapToJavaDrawableId(GetIconResourceID("offerTag"));
    }
    Java_AutofillKeyboardAccessoryViewBridge_addToAutofillSuggestionArray(
        env, data_array, position++,
        ConvertUTF16ToJavaString(env, controller_->GetSuggestionValueAt(i)),
        ConvertUTF16ToJavaString(env, controller_->GetSuggestionLabelAt(i)),
        ConvertUTF16ToJavaString(env, item_tag), android_icon_id,
        suggestion.frontend_id,
        controller_->GetRemovalConfirmationText(i, nullptr, nullptr));
  }
  Java_AutofillKeyboardAccessoryViewBridge_show(env, java_object_, data_array,
                                                controller_->IsRTL());
}

void AutofillKeyboardAccessoryView::ConfirmDeletion(
    const std::u16string& confirmation_title,
    const std::u16string& confirmation_body,
    base::OnceClosure confirm_deletion) {
  JNIEnv* env = base::android::AttachCurrentThread();
  confirm_deletion_ = std::move(confirm_deletion);
  Java_AutofillKeyboardAccessoryViewBridge_confirmDeletion(
      env, java_object_, ConvertUTF16ToJavaString(env, confirmation_title),
      ConvertUTF16ToJavaString(env, confirmation_body));
}

void AutofillKeyboardAccessoryView::SuggestionSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  controller_->AcceptSuggestion(list_index);
}

void AutofillKeyboardAccessoryView::DeletionRequested(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  controller_->RemoveSuggestion(list_index);
}

void AutofillKeyboardAccessoryView::DeletionConfirmed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (confirm_deletion_.is_null()) {
    LOG(DFATAL) << "DeletionConfirmed called but no deletion is pending!";
    return;
  }
  std::move(confirm_deletion_).Run();
}

void AutofillKeyboardAccessoryView::ViewDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  controller_->ViewDestroyed();
}

}  // namespace autofill
