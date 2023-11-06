// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/permissions/android/jni_headers/PermissionDialogController_jni.h"
#include "components/permissions/android/jni_headers/PermissionDialogDelegate_jni.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"
#include "components/permissions/permissions_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertUTF16ToJavaString;

namespace permissions {

PermissionDialogJavaDelegate::PermissionDialogJavaDelegate(
    PermissionPromptAndroid* permission_prompt)
    : permission_prompt_(permission_prompt) {}
PermissionDialogJavaDelegate::~PermissionDialogJavaDelegate() = default;

void PermissionDialogJavaDelegate::CreateJavaDelegate(
    content::WebContents* web_contents,
    PermissionDialogDelegate* owner) {
  // Create our Java counterpart, which manages the lifetime of
  // PermissionDialogDelegate.
  JNIEnv* env = base::android::AttachCurrentThread();
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
      env, reinterpret_cast<uintptr_t>(owner),
      web_contents->GetTopLevelNativeWindow()->GetJavaObject(),
      base::android::ToJavaIntArray(env, content_settings_types),
      PermissionsClient::Get()->MapToJavaDrawableId(
          permission_prompt_->GetIconId()),
      ConvertUTF16ToJavaString(env, permission_prompt_->GetMessageText()),
      primaryButtonText, secondaryButtonText));
}

void PermissionDialogJavaDelegate::CreateDialog(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Send the Java delegate to the Java PermissionDialogController for display.
  // The controller takes over lifetime management; when the Java delegate is no
  // longer needed it will in turn free the native delegate
  // (PermissionDialogDelegate).
  Java_PermissionDialogController_createDialog(env, j_delegate_);

  if (permission_prompt_->ShouldUseRequestingOriginFavicon()) {
    // In order to update the dialog, we need to make sure it has been created
    // before.
    GetAndUpdateRequestingOriginFavicon(web_contents);
  }
}

void PermissionDialogJavaDelegate::GetAndUpdateRequestingOriginFavicon(
    content::WebContents* web_contents) {
  favicon::FaviconService* favicon_service =
      PermissionsClient::Get()->GetFaviconService(
          web_contents->GetBrowserContext());
  CHECK(favicon_service);

  JNIEnv* env = base::android::AttachCurrentThread();
  int iconSizeInPx =
      Java_PermissionDialogDelegate_getIconSizeInPx(env, j_delegate_);

  // Fetching requesting origin favicon.
  // Fetch raw favicon to set |fallback_to_host|=true since we otherwise might
  // not get a result if the user never visited the root URL of |site|.
  favicon_service->GetRawFaviconForPageURL(
      permission_prompt_->GetRequestingOrigin(),
      {favicon_base::IconType::kFavicon}, iconSizeInPx,
      /*fallback_to_host=*/true,
      base::BindOnce(
          &PermissionDialogJavaDelegate::OnRequestingOriginFaviconLoaded,
          base::Unretained(this)),
      &favicon_tracker_);
}

void PermissionDialogJavaDelegate::OnRequestingOriginFaviconLoaded(
    const favicon_base::FaviconRawBitmapResult& favicon_result) {
  if (favicon_result.is_valid()) {
    gfx::Image image =
        gfx::Image::CreateFrom1xPNGBytes(favicon_result.bitmap_data->front(),
                                         favicon_result.bitmap_data->size());
    const SkBitmap* bitmap = image.ToSkBitmap();
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PermissionDialogDelegate_updateIcon(env, j_delegate_,
                                             gfx::ConvertToJavaBitmap(*bitmap));
  }
}

void PermissionDialogJavaDelegate::DismissDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PermissionDialogDelegate_dismissFromNative(env, j_delegate_);
}

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
  std::unique_ptr<PermissionDialogJavaDelegate> java_delegate(
      std::make_unique<PermissionDialogJavaDelegate>(permission_prompt));
  new PermissionDialogDelegate(web_contents, permission_prompt,
                               std::move(java_delegate));
}

// static
PermissionDialogDelegate* PermissionDialogDelegate::CreateForTesting(
    content::WebContents* web_contents,
    PermissionPromptAndroid* permission_prompt,
    std::unique_ptr<PermissionDialogJavaDelegate> java_delegate) {
  return new PermissionDialogDelegate(web_contents, permission_prompt,
                                      std::move(java_delegate));
}

void PermissionDialogDelegate::Accept(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  DCHECK(permission_prompt_);
  permission_prompt_->Accept();
}

void PermissionDialogDelegate::Cancel(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  DCHECK(permission_prompt_);
  permission_prompt_->Deny();
}

void PermissionDialogDelegate::Dismissed(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  DCHECK(permission_prompt_);
  permission_prompt_->Closing();
}

void PermissionDialogDelegate::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

PermissionDialogDelegate::PermissionDialogDelegate(
    content::WebContents* web_contents,
    PermissionPromptAndroid* permission_prompt,
    std::unique_ptr<PermissionDialogJavaDelegate> java_delegate)
    : content::WebContentsObserver(web_contents),
      permission_prompt_(permission_prompt),
      java_delegate_(std::move(java_delegate)) {
  DCHECK(java_delegate_);

  // Create our Java counterpart, which manages our lifetime.
  java_delegate_->CreateJavaDelegate(web_contents, this);
  // Open the Permission Dialog.
  java_delegate_->CreateDialog(web_contents);
}

PermissionDialogDelegate::~PermissionDialogDelegate() = default;

void PermissionDialogDelegate::DismissDialog() {
  java_delegate_->DismissDialog();
}

void PermissionDialogDelegate::PrimaryPageChanged(content::Page& page) {
  DismissDialog();
}

void PermissionDialogDelegate::WebContentsDestroyed() {
  DismissDialog();
}

static jint JNI_PermissionDialogDelegate_GetRequestTypeEnumSize(JNIEnv* env) {
  return static_cast<int>(RequestType::kMaxValue) + 1;
}

}  // namespace permissions
