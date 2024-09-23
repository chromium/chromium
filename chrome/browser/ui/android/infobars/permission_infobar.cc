// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/permission_infobar.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/permissions/permission_infobar_delegate_android.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PermissionInfoBar_jni.h"

namespace {

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ScopedJavaLocalRef<jobject> CreateRenderInfoBarHelper(
    JNIEnv* env,
    int enumerated_icon_id,
    const JavaRef<jobject>& window,
    const std::u16string& compact_message_text,
    const std::u16string& compact_link_text,
    const std::u16string& message_text,
    const std::u16string& description_text,
    const std::u16string& learn_more_link_text,
    const std::u16string& primary_button_text,
    const std::u16string& secondary_button_text,
    bool secondary_button_should_open_settings,
    const std::vector<int>& content_settings) {
  ScopedJavaLocalRef<jstring> compact_message_text_java =
      base::android::ConvertUTF16ToJavaString(env, compact_message_text);
  ScopedJavaLocalRef<jstring> compact_link_text_java =
      base::android::ConvertUTF16ToJavaString(env, compact_link_text);
  ScopedJavaLocalRef<jstring> message_text_java =
      base::android::ConvertUTF16ToJavaString(env, message_text);
  ScopedJavaLocalRef<jstring> description_text_java =
      base::android::ConvertUTF16ToJavaString(env, description_text);
  ScopedJavaLocalRef<jstring> learn_more_link_text_java =
      base::android::ConvertUTF16ToJavaString(env, learn_more_link_text);
  ScopedJavaLocalRef<jstring> primary_button_text_java =
      base::android::ConvertUTF16ToJavaString(env, primary_button_text);
  ScopedJavaLocalRef<jstring> secondary_button_text_java =
      base::android::ConvertUTF16ToJavaString(env, secondary_button_text);

  ScopedJavaLocalRef<jintArray> content_settings_types =
      base::android::ToJavaIntArray(env, content_settings);
  return Java_PermissionInfoBar_create(
      env, window, content_settings_types, enumerated_icon_id,
      compact_message_text_java, compact_link_text_java, message_text_java,
      description_text_java, learn_more_link_text_java,
      primary_button_text_java, secondary_button_text_java,
      secondary_button_should_open_settings);
}

}  // namespace

PermissionInfoBar::PermissionInfoBar(
    std::unique_ptr<PermissionInfoBarDelegate> delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)) {}

PermissionInfoBar::~PermissionInfoBar() {}

base::android::ScopedJavaLocalRef<jobject>
PermissionInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  PermissionInfoBarDelegate* delegate = GetDelegate();

  std::u16string compact_message_text = delegate->GetCompactMessageText();
  std::u16string compact_link_text = delegate->GetCompactLinkText();
  std::u16string message_text = delegate->GetMessageText();
  std::u16string description_text = delegate->GetDescriptionText();
  std::u16string learn_more_link_text = delegate->GetLinkText();
  std::u16string primary_button_text =
      GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK);
  std::u16string secondary_button_text =
      GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL);
  const bool secondary_button_should_open_settings =
      delegate->ShouldSecondaryButtonOpenSettings();

  int permission_icon =
      ResourceMapper::MapToJavaDrawableId(delegate->GetIconId());

  std::vector<int> content_settings_types;
  for (size_t i = 0; i < delegate->PermissionCount(); i++) {
    content_settings_types.push_back(
        static_cast<int>(delegate->GetContentSettingType(i)));
  }

  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(this);
  DCHECK(web_contents);

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);

  return CreateRenderInfoBarHelper(
      env, permission_icon,
      tab->web_contents()->GetTopLevelNativeWindow()->GetJavaObject(),
      compact_message_text, compact_link_text, message_text, description_text,
      learn_more_link_text, primary_button_text, secondary_button_text,
      secondary_button_should_open_settings, content_settings_types);
}

PermissionInfoBarDelegate* PermissionInfoBar::GetDelegate() {
  return static_cast<PermissionInfoBarDelegate*>(delegate());
}
