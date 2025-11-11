// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_install_dialog_view_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/modal_dialog_manager_bridge.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
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

void ExtensionInstallDialogViewAndroid::OnDialogAccepted(JNIEnv* env) {
  prompt_->OnDialogAccepted();
  std::move(done_callback_)
      .Run(ExtensionInstallPrompt::DoneCallbackPayload(
          ExtensionInstallPrompt::Result::ACCEPTED,
          /* justification= */ std::string()));
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

  Java_ExtensionInstallDialogBridge_withTitleAndButtons(
      env, java_object_,
      ConvertUTF16ToJavaString(env, prompt_->GetDialogTitle()),
      gfx::ConvertToJavaBitmap(prompt_->icon().AsBitmap()),
      ConvertUTF16ToJavaString(env, prompt_->GetAcceptButtonLabel()),
      ConvertUTF16ToJavaString(env, prompt_->GetAbortButtonLabel()));

  if (prompt_->GetPermissionCount() > 0) {
    ScopedJavaLocalRef<jstring> java_heading =
        ConvertUTF16ToJavaString(env, prompt_->GetPermissionsHeading());
    std::vector<std::u16string> permissions_text;
    std::vector<std::u16string> permissions_details;
    auto permissions = prompt_->GetPermissions();
    for (size_t i = 0; i < permissions.permissions.size(); ++i) {
      permissions_text.push_back(permissions.permissions[i]);
      permissions_details.push_back(permissions.details[i]);
    }

    ScopedJavaLocalRef<jobjectArray> java_permissions_text_array =
        base::android::ToJavaArrayOfStrings(env, permissions_text);
    ScopedJavaLocalRef<jobjectArray> java_permissions_details_array =
        base::android::ToJavaArrayOfStrings(env, permissions_details);
    Java_ExtensionInstallDialogBridge_withPermissions(
        env, java_object_, java_heading, java_permissions_text_array,
        java_permissions_details_array);
  }
}

}  // namespace extensions

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::BindRepeating(&ShowExtensionInstallDialogAndroid);
}
