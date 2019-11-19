// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/android/chrome_jni_headers/AutofillPopupBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/autofill_keyboard_accessory_adapter.h"
#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/security_state/core/security_state.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillPopupViewAndroid::AutofillPopupViewAndroid(
    AutofillPopupController* controller)
    : controller_(controller), deleting_index_(-1) {}

AutofillPopupViewAndroid::~AutofillPopupViewAndroid() {}

void AutofillPopupViewAndroid::Show() {
  OnSuggestionsChanged();
}

void AutofillPopupViewAndroid::Hide() {
  controller_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillPopupBridge_dismiss(env, java_object_);
  } else {
    // Hide() should delete |this| either via Java dismiss or directly.
    delete this;
  }
}

void AutofillPopupViewAndroid::OnSelectedRowChanged(
    base::Optional<int> previous_row_selection,
    base::Optional<int> current_row_selection) {}

void AutofillPopupViewAndroid::OnSuggestionsChanged() {
  if (java_object_.is_null())
    return;

  const ScopedJavaLocalRef<jobject> view = popup_view_.view();
  if (view.is_null())
    return;

  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);
  JNIEnv* env = base::android::AttachCurrentThread();
  view_android->SetAnchorRect(view, controller_->element_bounds());

  size_t count = controller_->GetLineCount();
  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AutofillPopupBridge_createAutofillSuggestionArray(env, count);

  for (size_t i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> value = base::android::ConvertUTF16ToJavaString(
        env, controller_->GetElidedValueAt(i));
    ScopedJavaLocalRef<jstring> label =
        base::android::ConvertUTF16ToJavaString(
            env, controller_->GetElidedLabelAt(i));
    int android_icon_id = 0;

    const Suggestion& suggestion = controller_->GetSuggestionAt(i);
    if (!suggestion.icon.empty()) {
      android_icon_id = ResourceMapper::MapFromChromiumId(
          controller_->layout_model().GetIconResourceID(suggestion.icon));
    }

    bool is_deletable =
        controller_->GetRemovalConfirmationText(i, nullptr, nullptr);
    bool is_label_multiline =
        suggestion.frontend_id ==
            POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE ||
        suggestion.frontend_id == POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO;
    Java_AutofillPopupBridge_addToAutofillSuggestionArray(
        env, data_array, i, value, label, android_icon_id,
        /*icon_at_start=*/false, suggestion.frontend_id, is_deletable,
        is_label_multiline, /*isLabelBold*/ false);
  }

  Java_AutofillPopupBridge_show(env, java_object_, data_array,
                                controller_->IsRTL());
}

base::Optional<int32_t> AutofillPopupViewAndroid::GetAxUniqueId() {
  NOTIMPLEMENTED() << "See https://crbug.com/985927";
  return base::nullopt;
}

void AutofillPopupViewAndroid::SuggestionSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  // Race: Hide() may have already run.
  if (controller_)
    controller_->AcceptSuggestion(list_index);
}

void AutofillPopupViewAndroid::DeletionRequested(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  if (!controller_ || java_object_.is_null())
    return;

  base::string16 confirmation_title, confirmation_body;
  if (!controller_->GetRemovalConfirmationText(list_index, &confirmation_title,
          &confirmation_body)) {
    return;
  }

  deleting_index_ = list_index;
  Java_AutofillPopupBridge_confirmDeletion(
      env, java_object_,
      base::android::ConvertUTF16ToJavaString(env, confirmation_title),
      base::android::ConvertUTF16ToJavaString(env, confirmation_body));
}

void AutofillPopupViewAndroid::DeletionConfirmed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!controller_)
    return;

  CHECK_GE(deleting_index_, 0);
  controller_->RemoveSuggestion(deleting_index_);
}

void AutofillPopupViewAndroid::PopupDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (controller_)
    controller_->ViewDestroyed();

  // The controller has now deleted itself. Remove dangling weak reference.
  controller_ = nullptr;
  delete this;
}

void AutofillPopupViewAndroid::Init() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);
  popup_view_ = view_android->AcquireAnchorView();
  const ScopedJavaLocalRef<jobject> view = popup_view_.view();
  if (view.is_null())
    return;

  java_object_.Reset(Java_AutofillPopupBridge_create(
      env, view, reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject()));
}

bool AutofillPopupViewAndroid::WasSuppressed() {
  return java_object_ &&
         Java_AutofillPopupBridge_wasSuppressed(
             base::android::AttachCurrentThread(), java_object_);
}

// static
AutofillPopupView* AutofillPopupView::Create(
    base::WeakPtr<AutofillPopupController> controller) {
  if (IsKeyboardAccessoryEnabled()) {
    auto adapter = std::make_unique<AutofillKeyboardAccessoryAdapter>(
        controller, GetKeyboardAccessoryAnimationDuration(),
        ShouldLimitKeyboardAccessorySuggestionLabelWidth());
    adapter->SetAccessoryView(
        std::make_unique<AutofillKeyboardAccessoryView>(adapter.get()));
    return adapter.release();
  }

  auto popup_view =
      std::make_unique<AutofillPopupViewAndroid>(controller.get());
  popup_view->Init();
  return popup_view->WasSuppressed() ? nullptr : popup_view.release();
}

}  // namespace autofill
