// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"

#include <string_view>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/jni_headers/PermissionDialogController_jni.h"
#include "components/permissions/android/jni_headers/PermissionDialogDelegate_jni.h"

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
  bool is_one_time = PermissionUtil::DoesSupportTemporaryGrants(
      permission_prompt_->GetContentSettingType(0));
  j_delegate_.Reset(Java_PermissionDialogDelegate_create(
      env, reinterpret_cast<uintptr_t>(owner),
      web_contents->GetTopLevelNativeWindow()->GetJavaObject(),
      permission_prompt_->GetContentSettingTypes(env),
      PermissionsClient::Get()->MapToJavaDrawableId(
          permission_prompt_->GetIconId()),
      ConvertUTF16ToJavaString(
          env, permission_prompt_->GetAnnotatedMessageText().text),
      permission_prompt_->GetBoldRanges(env),
      permission_prompt_->GetPositiveButtonText(env, is_one_time),
      permission_prompt_->GetNegativeButtonText(env, is_one_time),
      permission_prompt_->GetPositiveEphemeralButtonText(env, is_one_time),
      /*showPositiveNonEphemeralAsFirstButton=*/is_one_time,
      permission_prompt_->GetRadioButtonTexts(env, is_one_time),
      static_cast<int>(permission_prompt_->GetEmbeddedPromptVariant())));
}

void PermissionDialogJavaDelegate::CreateDialog(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Send the Java delegate to the Java PermissionDialogController for display.
  // When the Java delegate is no longer needed it will in turn reset the native
  // java delegate (PermissionDialogJavaDelegate).
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
        gfx::Image::CreateFrom1xPNGBytes(favicon_result.bitmap_data);
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

void PermissionDialogJavaDelegate::NotifyPermissionAllowed() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PermissionDialogDelegate_notifyPermissionAllowed(env, j_delegate_);
}

void PermissionDialogJavaDelegate::UpdateDialog() {
  CHECK(permission_prompt_->GetEmbeddedPromptVariant() !=
        EmbeddedPermissionPromptFlowModel::Variant::kUninitialized);
  JNIEnv* env = base::android::AttachCurrentThread();
  bool is_one_time = PermissionUtil::DoesSupportTemporaryGrants(
      permission_prompt_->GetContentSettingType(0));
  Java_PermissionDialogDelegate_updateDialog(
      env, j_delegate_, permission_prompt_->GetContentSettingTypes(env),
      PermissionsClient::Get()->MapToJavaDrawableId(
          permission_prompt_->GetIconId()),
      ConvertUTF16ToJavaString(
          env, permission_prompt_->GetAnnotatedMessageText().text),
      permission_prompt_->GetBoldRanges(env),
      permission_prompt_->GetPositiveButtonText(env, is_one_time),
      permission_prompt_->GetNegativeButtonText(env, is_one_time),
      permission_prompt_->GetPositiveEphemeralButtonText(env, is_one_time),
      /*showPositiveNonEphemeralAsFirstButton=*/is_one_time,
      permission_prompt_->GetRadioButtonTexts(env, is_one_time),
      static_cast<int>(permission_prompt_->GetEmbeddedPromptVariant()));
}

// static
std::unique_ptr<PermissionDialogDelegate> PermissionDialogDelegate::Create(
    content::WebContents* web_contents,
    PermissionPromptAndroid* permission_prompt) {
  CHECK(web_contents);
  // If we don't have a window, just act as though the prompt was dismissed.
  if (!web_contents->GetTopLevelNativeWindow()) {
    permission_prompt->Closing();
    return nullptr;
  }
  std::unique_ptr<PermissionDialogJavaDelegate> java_delegate(
      std::make_unique<PermissionDialogJavaDelegate>(permission_prompt));
  return std::make_unique<PermissionDialogDelegate>(
      web_contents, permission_prompt, std::move(java_delegate));
}

// static
std::unique_ptr<PermissionDialogDelegate>
PermissionDialogDelegate::CreateForTesting(
    content::WebContents* web_contents,
    PermissionPromptAndroid* permission_prompt,
    std::unique_ptr<PermissionDialogJavaDelegate> java_delegate) {
  return std::make_unique<PermissionDialogDelegate>(
      web_contents, permission_prompt, std::move(java_delegate));
}

void PermissionDialogDelegate::Accept(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  CHECK(permission_prompt_);
  permission_prompt_->Accept();
}

void PermissionDialogDelegate::AcceptThisTime(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(permission_prompt_);
  permission_prompt_->AcceptThisTime();
}

void PermissionDialogDelegate::Acknowledge(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  CHECK(permission_prompt_);
  permission_prompt_->Acknowledge();
}

void PermissionDialogDelegate::Deny(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  CHECK(permission_prompt_);
  permission_prompt_->Deny();
}

void PermissionDialogDelegate::Resumed(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  CHECK(permission_prompt_);
  permission_prompt_->Resumed();
}

void PermissionDialogDelegate::SystemSettingsShown(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(permission_prompt_);
  permission_prompt_->SystemSettingsShown();
}

void PermissionDialogDelegate::SystemPermissionResolved(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    bool accepted) {
  CHECK(permission_prompt_);
  permission_prompt_->SystemPermissionResolved(accepted);
}

void PermissionDialogDelegate::Dismissed(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         int dismissalType) {
  CHECK(permission_prompt_);
  std::vector<ContentSettingsType> content_settings_types;
  for (size_t i = 0; i < permission_prompt_->PermissionCount(); ++i) {
    ContentSettingsType type = permission_prompt_->GetContentSettingType(i);
    // Not all request types have an associated ContentSettingsType.
    if (type == ContentSettingsType::DEFAULT) {
      break;
    }
    content_settings_types.push_back(type);
  }

  if (content_settings_types.size() == permission_prompt_->PermissionCount()) {
    PermissionUmaUtil::RecordDismissalType(
        content_settings_types, permission_prompt_->GetPromptDisposition(),
        static_cast<DismissalType>(dismissalType));
  }

  if (!permission_prompt_->IsShowing()) {
    // This probably happens synchronously when creating the
    // `PermissionPromptAndroid` fails, and the `view_` of
    // `PermissionRequestManager` won't be ready yet. It can mess up here, this
    // prompt will be assigned to the 'view_' of the 'PermissionRequestManager.'
    // But, all the underlying data associated with it will get wiped.
    // So, we destroy the Java delegate and use the `IsJavaDelegateDestroyed`
    // signal as a way to tell if the `PermissionPrompt` creation failed.
    DestroyJavaDelegate();
  }
  permission_prompt_->Closing();
}

void PermissionDialogDelegate::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  DestroyJavaDelegate();
}

void PermissionDialogDelegate::NotifyPermissionAllowed() {
  CHECK(!IsJavaDelegateDestroyed());
  java_delegate_->NotifyPermissionAllowed();
}

void PermissionDialogDelegate::UpdateDialog() {
  CHECK(!IsJavaDelegateDestroyed());
  java_delegate_->UpdateDialog();
}

PermissionDialogDelegate::PermissionDialogDelegate(
    content::WebContents* web_contents,
    PermissionPromptAndroid* permission_prompt,
    std::unique_ptr<PermissionDialogJavaDelegate> java_delegate)
    : content::WebContentsObserver(web_contents),
      permission_prompt_(permission_prompt),
      java_delegate_(std::move(java_delegate)) {
  CHECK(java_delegate_);

  // Create our Java counterpart.
  java_delegate_->CreateJavaDelegate(web_contents, this);
  // Open the Permission Dialog.
  java_delegate_->CreateDialog(web_contents);
  // Note: `java_delegate_` can be destroyed after this line, if Java
  // counterpart fails to show the dialog.
}

PermissionDialogDelegate::~PermissionDialogDelegate() {
  // When the owning class is destroyed, ensure that any active java delegate
  // associated with the class is destroyed.
  if (!IsJavaDelegateDestroyed()) {
    DismissDialog();
  }
}

void PermissionDialogDelegate::DismissDialog() {
  // `java_delegate_` is owned by `this` and will be freed before `this`. During
  // the gap, it's still possible that `this` receives some dismiss signals but
  // should do nothing.
  if (!IsJavaDelegateDestroyed()) {
    java_delegate_->DismissDialog();
  }
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
