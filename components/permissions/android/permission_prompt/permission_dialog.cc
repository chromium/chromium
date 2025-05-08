// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_dialog.h"

#include "base/android/jni_string.h"
#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace permissions {

using base::android::ConvertUTF16ToJavaString;

PermissionDialog::PermissionDialog(content::WebContents* web_contents,
                                   Delegate* delegate)
    : PermissionPromptAndroid(web_contents, delegate) {
  DCHECK(web_contents);
  CreatePermissionDialogDelegate();
}

PermissionDialog::~PermissionDialog() = default;

// static
std::unique_ptr<PermissionDialog> PermissionDialog::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  auto prompt = std::make_unique<PermissionDialog>(web_contents, delegate);
  if (!prompt->permission_dialog_delegate() ||
      prompt->permission_dialog_delegate()->IsJavaDelegateDestroyed()) {
    return nullptr;
  }
  return prompt;
}

PermissionPromptDisposition PermissionDialog::GetPromptDisposition() const {
  return PermissionPromptDisposition::MODAL_DIALOG;
}

base::android::ScopedJavaLocalRef<jstring>
PermissionDialog::GetPositiveButtonText(JNIEnv* env, bool is_one_time) const {
  return is_one_time
             ? ConvertUTF16ToJavaString(
                   env, l10n_util::GetStringUTF16(
                            IDS_PERMISSION_ALLOW_WHILE_VISITING))
             : ConvertUTF16ToJavaString(
                   env, l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
}
base::android::ScopedJavaLocalRef<jstring>
PermissionDialog::GetNegativeButtonText(JNIEnv* env, bool is_one_time) const {
  return is_one_time
             ? ConvertUTF16ToJavaString(
                   env, l10n_util::GetStringUTF16(IDS_PERMISSION_NEVER_ALLOW))
             : ConvertUTF16ToJavaString(
                   env, l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
}
base::android::ScopedJavaLocalRef<jstring>
PermissionDialog::GetPositiveEphemeralButtonText(JNIEnv* env,
                                                 bool is_one_time) const {
  return is_one_time
             ? ConvertUTF16ToJavaString(
                   env,
                   l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME))
             : ConvertUTF16ToJavaString(env, std::u16string_view());
  ;
}

}  // namespace permissions
