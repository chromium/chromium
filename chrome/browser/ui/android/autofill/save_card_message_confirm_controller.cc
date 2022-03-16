// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_card_message_confirm_controller.h"

#include "chrome/android/chrome_jni_headers/AutofillMessageConfirmFlowBridge_jni.h"

#include "components/messages/android/messages_feature.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ScopedJavaLocalRef;

namespace autofill {

// Get the Save Card confirmation dialog title resource ID depending on the
// version of the dialog. See crbug.com/1306294 for details.
int GetSaveCardDialogTitleId() {
  if (messages::IsSaveCardMessagesUiEnabled() &&
      messages::UseDialogV2ForSaveCardMessage()) {
    return IDS_AUTOFILL_MOBILE_SAVE_CARD_TO_CLOUD_CONFIRMATION_DIALOG_TITLE_V2;
  }
  return IDS_AUTOFILL_MOBILE_SAVE_CARD_TO_CLOUD_CONFIRMATION_DIALOG_TITLE;
}

SaveCardMessageConfirmController::SaveCardMessageConfirmController(
    SaveCardMessageConfirmDelegate* delegate,
    content::WebContents* web_contents)
    : delegate_(delegate), web_contents_(web_contents) {}

SaveCardMessageConfirmController::~SaveCardMessageConfirmController() {
  if (java_object_) {
    Java_AutofillMessageConfirmFlowBridge_dismiss(
        base::android::AttachCurrentThread(), GetOrCreateJavaObject());
    Java_AutofillMessageConfirmFlowBridge_nativeBridgeDestroyed(
        base::android::AttachCurrentThread(), GetOrCreateJavaObject());
  }
}

void SaveCardMessageConfirmController::ConfirmSaveCard(
    const std::u16string& card_label) {
  if (!GetOrCreateJavaObject())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillMessageConfirmFlowBridge_confirmSaveCard(
      env, GetOrCreateJavaObject(),
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(GetSaveCardDialogTitleId())),
      base::android::ConvertUTF16ToJavaString(env, card_label),
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FIX_FLOW_PROMPT_SAVE_CARD_LABEL)));
}

void SaveCardMessageConfirmController::FixName(
    const std::u16string& inferred_cardholder_name,
    const std::u16string& card_label) {
  if (!GetOrCreateJavaObject())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillMessageConfirmFlowBridge_dismiss(env, GetOrCreateJavaObject());
  Java_AutofillMessageConfirmFlowBridge_fixName(
      env, GetOrCreateJavaObject(),
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(GetSaveCardDialogTitleId())),
      base::android::ConvertUTF16ToJavaString(env, inferred_cardholder_name),
      base::android::ConvertUTF16ToJavaString(env, card_label),
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FIX_FLOW_PROMPT_SAVE_CARD_LABEL)));
}

void SaveCardMessageConfirmController::FixDate(
    const std::u16string& card_label) {
  if (!GetOrCreateJavaObject())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillMessageConfirmFlowBridge_fixDate(
      env, GetOrCreateJavaObject(),
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(GetSaveCardDialogTitleId())),
      base::android::ConvertUTF16ToJavaString(env, card_label),
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_FIX_FLOW_PROMPT_SAVE_CARD_LABEL)));
}

void SaveCardMessageConfirmController::AddLegalMessageLine(
    const LegalMessageLine& line) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillMessageConfirmFlowBridge_addLegalMessageLine(
      env, java_object,
      base::android::ConvertUTF16ToJavaString(env, line.text()));
  for (const auto& link : line.links()) {
    Java_AutofillMessageConfirmFlowBridge_addLinkToLastLegalMessageLine(
        env, java_object, link.range.start(), link.range.end(),
        base::android::ConvertUTF8ToJavaString(env, link.url.spec()));
  }
}

base::android::ScopedJavaGlobalRef<jobject>
SaveCardMessageConfirmController::GetOrCreateJavaObject() {
  if (java_object_)
    return java_object_;

  if (web_contents_->GetNativeView() == nullptr ||
      web_contents_->GetNativeView()->GetWindowAndroid() == nullptr)
    return nullptr;  // No window attached (yet or anymore).

  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  return java_object_ = Java_AutofillMessageConfirmFlowBridge_create(
             env, reinterpret_cast<intptr_t>(delegate_.get()),
             view_android->GetWindowAndroid()->GetJavaObject());
}

void SaveCardMessageConfirmController::DismissDialog() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  Java_AutofillMessageConfirmFlowBridge_dismiss(
      base::android::AttachCurrentThread(), java_object);
}

}  // namespace autofill
