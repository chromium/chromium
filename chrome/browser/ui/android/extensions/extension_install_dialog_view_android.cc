// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_install_dialog_view_android.h"

#include <jni.h>

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/modal_dialog_manager_bridge.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionInstallDialogBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

void ShowExtensionInstallDialogAndroid(
    std::unique_ptr<ExtensionInstallPromptShowParams> show_params,
    ExtensionInstallPrompt::DoneCallback done_callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  content::WebContents* web_contents = show_params->GetParentWebContents();
  if (!web_contents) {
    return;
  }

  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  DCHECK(view_android);
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return;
  }

  auto* dialog_view = new extensions::ExtensionInstallDialogViewAndroid(
      std::move(prompt), std::move(done_callback));
  dialog_view->ShowDialog(window_android);
  // `dialog_view` will delete itself when dialog is dismissed.
}

}  // namespace

namespace extensions {

ExtensionInstallDialogViewAndroid::ExtensionInstallDialogViewAndroid(
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt,
    ExtensionInstallPrompt::DoneCallback done_callback)
    : prompt_(std::move(prompt)), done_callback_(std::move(done_callback)) {}

ExtensionInstallDialogViewAndroid::~ExtensionInstallDialogViewAndroid() {
  if (!done_callback_) {
    return;
  }

  prompt_->OnDialogCanceled();
  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
}

void ExtensionInstallDialogViewAndroid::ShowDialog(
    ui::WindowAndroid* window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_ExtensionInstallDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));

  BuildPropertyModel();
  Java_ExtensionInstallDialogBridge_showDialog(env, java_object_);
}

void ExtensionInstallDialogViewAndroid::OnDialogAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& justification_text) {
  std::string justification =
      base::android::ConvertJavaStringToUTF8(env, justification_text);
  prompt_->OnDialogAccepted();
  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::ACCEPTED, justification));
}

void ExtensionInstallDialogViewAndroid::OnDialogCanceled(JNIEnv* env) {
  OnDialogDismissed(env);
}

void ExtensionInstallDialogViewAndroid::OnDialogDismissed(JNIEnv* env) {
  prompt_->OnDialogCanceled();
  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::USER_CANCELED));
}

void ExtensionInstallDialogViewAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void ExtensionInstallDialogViewAndroid::BuildPropertyModel() {
  JNIEnv* env = base::android::AttachCurrentThread();

  bool has_permissions = prompt_->GetPermissionCount() > 0;
  if (has_permissions) {
    std::u16string permissions_heading = prompt_->GetPermissionsHeading();
    std::vector<std::u16string> permissions_text;
    std::vector<std::u16string> permissions_details;

    auto permissions = prompt_->GetPermissions();
    for (size_t i = 0; i < permissions.permissions.size(); ++i) {
      permissions_text.push_back(permissions.permissions[i]);
      permissions_details.push_back(permissions.details[i]);
    }

    ScopedJavaLocalRef<jstring> java_permissions_heading =
        ConvertUTF16ToJavaString(env, permissions_heading);
    ScopedJavaLocalRef<jobjectArray> java_permissions_text_array =
        base::android::ToJavaArrayOfStrings(env, permissions_text);
    ScopedJavaLocalRef<jobjectArray> java_permissions_details_array =
        base::android::ToJavaArrayOfStrings(env, permissions_details);
    Java_ExtensionInstallDialogBridge_withPermissions(
        env, java_object_, java_permissions_heading,
        java_permissions_text_array, java_permissions_details_array);
  }

  bool requires_justification =
      prompt_->type() ==
      ExtensionInstallPrompt::PromptType::EXTENSION_REQUEST_PROMPT;
  if (requires_justification) {
    std::u16string justification_heading = l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_EXTENSION_REQUEST_JUSTIFICATION);
    std::u16string justification_placeholder = l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_EXTENSION_REQUEST_JUSTIFICATION_PLACEHOLDER);

    ScopedJavaLocalRef<jstring> java_justification_heading =
        ConvertUTF16ToJavaString(env, justification_heading);
    ScopedJavaLocalRef<jstring> java_justification_placeholder =
        ConvertUTF16ToJavaString(env, justification_placeholder);
    Java_ExtensionInstallDialogBridge_withJustification(
        env, java_object_, java_justification_heading,
        java_justification_placeholder);
  }

  Java_ExtensionInstallDialogBridge_buildDialog(
      env, java_object_,
      ConvertUTF16ToJavaString(env, prompt_->GetDialogTitle()),
      gfx::ConvertToJavaBitmap(prompt_->icon().AsBitmap()),
      ConvertUTF16ToJavaString(env, prompt_->GetAcceptButtonLabel()),
      ConvertUTF16ToJavaString(env, prompt_->GetAbortButtonLabel()));
}

}  // namespace extensions

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::BindRepeating(&ShowExtensionInstallDialogAndroid);
}

DEFINE_JNI(ExtensionInstallDialogBridge)
