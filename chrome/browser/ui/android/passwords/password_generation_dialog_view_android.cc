// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/password_generation_dialog_view_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/PasswordGenerationDialogBridge_jni.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_generation_controller_impl.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::metrics_util::GenerationDialogChoice;

PasswordGenerationDialogViewAndroid::PasswordGenerationDialogViewAndroid(
    PasswordGenerationController* controller)
    : controller_(controller) {
  ui::WindowAndroid* window_android = controller_->top_level_native_window();

  DCHECK(window_android);
  java_object_.Reset(Java_PasswordGenerationDialogBridge_create(
      base::android::AttachCurrentThread(), window_android->GetJavaObject(),
      reinterpret_cast<intptr_t>(this)));
}

PasswordGenerationDialogViewAndroid::~PasswordGenerationDialogViewAndroid() {
  DCHECK(!java_object_.is_null());
  Java_PasswordGenerationDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void PasswordGenerationDialogViewAndroid::Show(
    std::u16string& password,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver>
        target_frame_driver,
    autofill::password_generation::PasswordGenerationType type) {
  generation_type_ = type;
  target_frame_driver_ = std::move(target_frame_driver);
  JNIEnv* env = base::android::AttachCurrentThread();

  Profile* profile = Profile::FromBrowserContext(
      controller_->web_contents()->GetBrowserContext());
  absl::optional<AccountInfo> account_info =
      password_manager::GetAccountInfoForPasswordMessages(profile);

  std::u16string explanation_text;
  if (account_info.has_value()) {
    explanation_text = l10n_util::GetStringFUTF16(
        IDS_PASSWORD_GENERATION_DIALOG_DESCRIPTION_BRANDED,
        base::UTF8ToUTF16(account_info.value().email));
  } else {
    explanation_text =
        l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROMPT);
  }

  Java_PasswordGenerationDialogBridge_showDialog(
      env, java_object_, base::android::ConvertUTF16ToJavaString(env, password),
      base::android::ConvertUTF16ToJavaString(env, explanation_text));
}

void PasswordGenerationDialogViewAndroid::PasswordAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& password) {
  controller_->GeneratedPasswordAccepted(
      base::android::ConvertJavaStringToUTF16(env, password),
      std::move(target_frame_driver_), generation_type_);
}

void PasswordGenerationDialogViewAndroid::PasswordRejected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->GeneratedPasswordRejected(generation_type_);
}

// static
std::unique_ptr<PasswordGenerationDialogViewInterface>
PasswordGenerationDialogViewInterface::Create(
    PasswordGenerationController* controller) {
  return std::make_unique<PasswordGenerationDialogViewAndroid>(controller);
}
