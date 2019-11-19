// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/grouped_permission_infobar.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/PermissionInfoBar_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"

namespace {

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ScopedJavaLocalRef<jobject> CreateRenderInfoBarHelper(
    JNIEnv* env,
    int enumerated_icon_id,
    const JavaRef<jobject>& tab,
    const base::string16& compact_message_text,
    const base::string16& compact_link_text,
    const base::string16& message_text,
    const base::string16& description_text,
    const base::string16& ok_button_text,
    const base::string16& cancel_button_text,
    const std::vector<int>& content_settings) {
  ScopedJavaLocalRef<jstring> compact_message_text_java =
      base::android::ConvertUTF16ToJavaString(env, compact_message_text);
  ScopedJavaLocalRef<jstring> compact_link_text_java =
      base::android::ConvertUTF16ToJavaString(env, compact_link_text);
  ScopedJavaLocalRef<jstring> message_text_java =
      base::android::ConvertUTF16ToJavaString(env, message_text);
  ScopedJavaLocalRef<jstring> description_text_java =
      base::android::ConvertUTF16ToJavaString(env, description_text);
  ScopedJavaLocalRef<jstring> ok_button_text_java =
      base::android::ConvertUTF16ToJavaString(env, ok_button_text);
  ScopedJavaLocalRef<jstring> cancel_button_text_java =
      base::android::ConvertUTF16ToJavaString(env, cancel_button_text);

  ScopedJavaLocalRef<jintArray> content_settings_types =
      base::android::ToJavaIntArray(env, content_settings);
  return Java_PermissionInfoBar_create(
      env, tab, content_settings_types, enumerated_icon_id,
      compact_message_text_java, compact_link_text_java, message_text_java,
      description_text_java, ok_button_text_java, cancel_button_text_java);
}

}  // namespace

GroupedPermissionInfoBar::GroupedPermissionInfoBar(
    std::unique_ptr<GroupedPermissionInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

GroupedPermissionInfoBar::~GroupedPermissionInfoBar() {}

base::android::ScopedJavaLocalRef<jobject>
GroupedPermissionInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  GroupedPermissionInfoBarDelegate* delegate = GetDelegate();

  base::string16 compact_message_text = delegate->GetCompactMessageText();
  base::string16 compact_link_text = delegate->GetCompactLinkText();
  base::string16 message_text = delegate->GetMessageText();
  base::string16 description_text = delegate->GetDescriptionText();
  base::string16 ok_button_text = GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK);
  base::string16 cancel_button_text =
      GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL);

  int permission_icon =
      ResourceMapper::MapFromChromiumId(delegate->GetIconId());

  std::vector<int> content_settings_types;
  for (size_t i = 0; i < delegate->PermissionCount(); i++) {
    content_settings_types.push_back(
        static_cast<int>(delegate->GetContentSettingType(i)));
  }

  return CreateRenderInfoBarHelper(
      env, permission_icon, GetTab()->GetJavaObject(), compact_message_text,
      compact_link_text, message_text, description_text, ok_button_text,
      cancel_button_text, content_settings_types);
}

GroupedPermissionInfoBarDelegate* GroupedPermissionInfoBar::GetDelegate() {
  return static_cast<GroupedPermissionInfoBarDelegate*>(delegate());
}
