// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_dialog_delegate.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/permissions/android/jni_headers/PermissionDialogController_jni.h"
#include "components/permissions/android/jni_headers/PermissionDialogDelegate_jni.h"
#include "components/permissions/permissions_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF16ToJavaString;

namespace permissions {

// static
void PermissionDialogDelegate::Create(
    content::WebContents* web_contents,
    PermissionPromptAndroid* permission_prompt) {
  DCHECK(web_contents);

  // If we don't have a window, just act as though the prompt was dismissed.
  if (!web_contents->GetTopLevelNativeWindow()) {
    permission_prompt->Closing();
    return;
  }

  // Dispatch the dialog to Java, which manages the lifetime of this object.
  new PermissionDialogDelegate(web_contents, permission_prompt);
}

void PermissionDialogDelegate::CreateJavaDelegate(
    JNIEnv* env,
    content::WebContents* web_contents) {
  base::android::ScopedJavaLocalRef<jstring> primaryButtonText =
      ConvertUTF16ToJavaString(env,
                               l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  base::android::ScopedJavaLocalRef<jstring> secondaryButtonText =
      ConvertUTF16ToJavaString(env,
                               l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));

  std::vector<int> content_settings_types;
  for (size_t i = 0; i < permission_prompt_->PermissionCount(); ++i) {
    content_settings_types.push_back(
        static_cast<int>(permission_prompt_->GetContentSettingType(i)));
  }

  j_delegate_.Reset(Java_PermissionDialogDelegate_create(
      env, reinterpret_cast<uintptr_t>(this),
      web_contents->GetTopLevelNativeWindow()->GetJavaObject(),
      base::android::ToJavaIntArray(env, content_settings_types),
      PermissionsClient::Get()->MapToJavaDrawableId(
          permission_prompt_->GetIconId()),
      ConvertUTF16ToJavaString(env, permission_prompt_->GetMessageText()),
      primaryButtonText, secondaryButtonText));
}

void PermissionDialogDelegate::Accept(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  permission_prompt_->Accept();
}

void PermissionDialogDelegate::Cancel(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  permission_prompt_->Deny();
}

void PermissionDialogDelegate::Dismissed(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  permission_prompt_->Closing();
}

void PermissionDialogDelegate::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

PermissionDialogDelegate::PermissionDialogDelegate(
    content::WebContents* web_contents,
    PermissionPromptAndroid* permission_prompt)
    : content::WebContentsObserver(web_contents),
      permission_prompt_(permission_prompt) {
  DCHECK(permission_prompt_);

  // Create our Java counterpart, which manages our lifetime.
  JNIEnv* env = base::android::AttachCurrentThread();
  CreateJavaDelegate(env, web_contents);

  // Send the Java delegate to the Java PermissionDialogController for display.
  // The controller takes over lifetime management; when the Java delegate is no
  // longer needed it will in turn free the native delegate.
  Java_PermissionDialogController_createDialog(env, j_delegate_);
}

PermissionDialogDelegate::~PermissionDialogDelegate() {}

void PermissionDialogDelegate::DismissDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PermissionDialogDelegate_dismissFromNative(env, j_delegate_);
}

void PermissionDialogDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  DismissDialog();
}

void PermissionDialogDelegate::WebContentsDestroyed() {
  DismissDialog();
}

static jint JNI_PermissionDialogDelegate_GetRequestTypeEnumSize(JNIEnv* env) {
  return static_cast<int>(RequestType::kMaxValue) + 1;
}

}  // namespace permissions
