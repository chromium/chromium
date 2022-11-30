// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/installable/installable_ambient_badge_infobar.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_infobar_delegate.h"
#include "components/webapps/browser/android/webapps_jni_headers/InstallableAmbientBadgeInfoBar_jni.h"
#include "ui/gfx/android/java_bitmap.h"

namespace webapps {

InstallableAmbientBadgeInfoBar::InstallableAmbientBadgeInfoBar(
    std::unique_ptr<InstallableAmbientBadgeInfoBarDelegate> delegate)
    : infobars::InfoBarAndroid(std::move(delegate)) {}

InstallableAmbientBadgeInfoBar::~InstallableAmbientBadgeInfoBar() = default;

InstallableAmbientBadgeInfoBarDelegate*
InstallableAmbientBadgeInfoBar::GetDelegate() {
  return static_cast<InstallableAmbientBadgeInfoBarDelegate*>(delegate());
}

void InstallableAmbientBadgeInfoBar::AddToHomescreen(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  GetDelegate()->AddToHomescreen();
  RemoveSelf();
}

base::android::ScopedJavaLocalRef<jobject>
InstallableAmbientBadgeInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  InstallableAmbientBadgeInfoBarDelegate* delegate = GetDelegate();
  base::android::ScopedJavaLocalRef<jstring> java_message_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetMessageText());
  base::android::ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, delegate->GetUrl().spec());

  DCHECK(!delegate->GetPrimaryIcon().drawsNothing());
  base::android::ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(delegate->GetPrimaryIcon());

  jboolean java_is_primary_icon_maskable = delegate->GetIsPrimaryIconMaskable();

  return Java_InstallableAmbientBadgeInfoBar_show(
      env, delegate->GetIconId(), java_bitmap, java_message_text, java_url,
      java_is_primary_icon_maskable);
}

void InstallableAmbientBadgeInfoBar::ProcessButton(int action) {}

}  // namespace webapps
