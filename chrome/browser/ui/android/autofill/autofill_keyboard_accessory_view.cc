// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"

#include "chrome/android/features/keyboard_accessory/jni_headers/AutofillKeyboardAccessoryViewBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/autofill_popup_controller_utils.h"
#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/android/gurl_android.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillKeyboardAccessoryView::AutofillKeyboardAccessoryView(
    base::WeakPtr<AutofillPopupController> controller)
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
  TRACE_EVENT0("passwords", "AutofillKeyboardAccessoryView::Hide");
  Java_AutofillKeyboardAccessoryViewBridge_dismiss(
      base::android::AttachCurrentThread(), java_object_);
}

void AutofillKeyboardAccessoryView::Show() {
  TRACE_EVENT0("passwords", "AutofillKeyboardAccessoryView::Show");
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

    std::u16string value;
    std::u16string label;
    if (controller_->GetSuggestionMinorTextAt(i).empty()) {
      value = controller_->GetSuggestionMainTextAt(i);
      std::vector<std::vector<autofill::Suggestion::Text>> suggestion_labels =
          controller_->GetSuggestionLabelsAt(i);
      if (!suggestion_labels.empty()) {
        DCHECK_EQ(suggestion_labels[0].size(), 1U);
        label = std::move(suggestion_labels[0][0].value);
      }
    } else {
      value = controller_->GetSuggestionMainTextAt(i);
      label = controller_->GetSuggestionMinorTextAt(i);
    }

    Java_AutofillKeyboardAccessoryViewBridge_addToAutofillSuggestionArray(
        env, data_array, position++, ConvertUTF16ToJavaString(env, value),
        ConvertUTF16ToJavaString(env, label), android_icon_id,
        suggestion.frontend_id,
        controller_->GetRemovalConfirmationText(i, nullptr, nullptr),
        ConvertUTF8ToJavaString(env, suggestion.feature_for_iph),
        url::GURLAndroid::FromNativeGURL(env, suggestion.custom_icon_url));
  }
  Java_AutofillKeyboardAccessoryViewBridge_show(env, java_object_, data_array,
                                                controller_->IsRTL());
}

void AutofillKeyboardAccessoryView::AxAnnounce(const std::u16string& text) {
  AnnounceTextForA11y(text);
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
