// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/autofill/android/payments/legal_message_line_android.h"
#include "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillSaveCardBottomSheetBridge_jni.h"
#include "components/autofill/android/payments_jni_headers/AutofillSaveCardUiInfo_jni.h"

namespace autofill {

namespace {

static base::android::ScopedJavaLocalRef<jobject> ConvertUiInfoToJavaObject(
    JNIEnv* env,
    const AutofillSaveCardUiInfo& ui_info) {
  // LINT.IfChange
  return Java_AutofillSaveCardUiInfo_Constructor(
      env, ui_info.is_for_upload,
      ResourceMapper::MapToJavaDrawableId(ui_info.logo_icon_id),
      ResourceMapper::MapToJavaDrawableId(ui_info.issuer_icon_id),
      LegalMessageLineAndroid::ConvertToJavaLinkedList(
          ui_info.legal_message_lines),
      base::android::ConvertUTF16ToJavaString(env, ui_info.card_label),
      base::android::ConvertUTF16ToJavaString(env, ui_info.card_sub_label),
      base::android::ConvertUTF16ToJavaString(env, ui_info.card_description),
      base::android::ConvertUTF16ToJavaString(env, ui_info.title_text),
      base::android::ConvertUTF16ToJavaString(env, ui_info.confirm_text),
      base::android::ConvertUTF16ToJavaString(env, ui_info.cancel_text),
      base::android::ConvertUTF16ToJavaString(env, ui_info.description_text),
      base::android::ConvertUTF16ToJavaString(env, ui_info.loading_description),
      ui_info.is_google_pay_branding_enabled);
  // LINT.ThenChange(//components/autofill/android/java/src/org/chromium/components/autofill/payments/AutofillSaveCardUiInfo.java)
}

}  // namespace

AutofillSaveCardBottomSheetBridge::AutofillSaveCardBottomSheetBridge(
    ui::WindowAndroid* window_android,
    TabModel* tab_model) {
  CHECK(window_android);
  CHECK(tab_model);
  java_autofill_save_card_bottom_sheet_bridge_ =
      Java_AutofillSaveCardBottomSheetBridge_Constructor(
          base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this),
          window_android->GetJavaObject(), tab_model->GetJavaObject());
}

AutofillSaveCardBottomSheetBridge::~AutofillSaveCardBottomSheetBridge() {
  if (java_autofill_save_card_bottom_sheet_bridge_) {
    Java_AutofillSaveCardBottomSheetBridge_destroy(
        base::android::AttachCurrentThread(),
        java_autofill_save_card_bottom_sheet_bridge_);
  }
}

void AutofillSaveCardBottomSheetBridge::RequestShowContent(
    const AutofillSaveCardUiInfo& ui_info,
    std::unique_ptr<AutofillSaveCardDelegateAndroid> delegate) {
  // Skip loading if additional fix flows are needed after save.
  bool skip_loading = delegate->requires_fix_flow();

  JNIEnv* env = base::android::AttachCurrentThread();
  save_card_delegate_ = std::move(delegate);
  Java_AutofillSaveCardBottomSheetBridge_requestShowContent(
      env, java_autofill_save_card_bottom_sheet_bridge_,
      ConvertUiInfoToJavaObject(env, ui_info), skip_loading);
}

void AutofillSaveCardBottomSheetBridge::Hide() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillSaveCardBottomSheetBridge_hide(
      env, java_autofill_save_card_bottom_sheet_bridge_);
}

AutofillSaveCardBottomSheetBridge::AutofillSaveCardBottomSheetBridge(
    base::android::ScopedJavaGlobalRef<jobject>
        java_autofill_save_card_bottom_sheet_bridge)
    : java_autofill_save_card_bottom_sheet_bridge_(
          java_autofill_save_card_bottom_sheet_bridge) {}

void AutofillSaveCardBottomSheetBridge::OnUiShown(JNIEnv* env) {
  if (save_card_delegate_) {
    save_card_delegate_->OnUiShown();
  }
}

void AutofillSaveCardBottomSheetBridge::OnUiAccepted(JNIEnv* env) {
  if (save_card_delegate_) {
    save_card_delegate_->OnUiAccepted(base::BindOnce(
        &AutofillSaveCardBottomSheetBridge::ResetSaveCardDelegate,
        base::Unretained(this)));
  }
}

void AutofillSaveCardBottomSheetBridge::OnUiCanceled(JNIEnv* env) {
  if (save_card_delegate_) {
    save_card_delegate_->OnUiCanceled();
  }
  ResetSaveCardDelegate();
}

void AutofillSaveCardBottomSheetBridge::OnUiIgnored(JNIEnv* env) {
  if (save_card_delegate_) {
    save_card_delegate_->OnUiIgnored();
  }
  ResetSaveCardDelegate();
}

void AutofillSaveCardBottomSheetBridge::ResetSaveCardDelegate() {
  save_card_delegate_.reset(nullptr);
}

}  // namespace autofill
