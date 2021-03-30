// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/manual_filling_view_android.h"

#include <jni.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/features/keyboard_accessory/jni_headers/ManualFillingComponentBridge_jni.h"
#include "chrome/android/features/keyboard_accessory/jni_headers/UserInfoField_jni.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_controller_impl.h"
#include "chrome/browser/password_manager/android/password_accessory_metrics_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using autofill::AccessorySheetData;
using autofill::FooterCommand;
using autofill::UserInfo;
using autofill::password_generation::PasswordGenerationUIData;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using password_manager::PasswordForm;

ManualFillingViewAndroid::ManualFillingViewAndroid(
    ManualFillingController* controller)
    : controller_(controller) {}

ManualFillingViewAndroid::~ManualFillingViewAndroid() {
  if (!java_object_internal_)
    return;  // No work to do.
  Java_ManualFillingComponentBridge_destroy(
      base::android::AttachCurrentThread(), java_object_internal_);
  java_object_internal_.Reset(nullptr);
}

void ManualFillingViewAndroid::OnItemsAvailable(
    const AccessorySheetData& data) {
  if (auto obj = GetOrCreateJavaObject()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ManualFillingComponentBridge_onItemsAvailable(
        env, obj, ConvertAccessorySheetDataToJavaObject(env, data));
  }
}

void ManualFillingViewAndroid::CloseAccessorySheet() {
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_closeAccessorySheet(
        base::android::AttachCurrentThread(), obj);
  }
}

void ManualFillingViewAndroid::SwapSheetWithKeyboard() {
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_swapSheetWithKeyboard(
        base::android::AttachCurrentThread(), obj);
  }
}

void ManualFillingViewAndroid::ShowWhenKeyboardIsVisible() {
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_showWhenKeyboardIsVisible(
        base::android::AttachCurrentThread(), obj);
  }
}

void ManualFillingViewAndroid::Hide() {
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_hide(base::android::AttachCurrentThread(),
                                           obj);
  }
}

void ManualFillingViewAndroid::OnAutomaticGenerationStatusChanged(
    bool available) {
  if (!available && java_object_internal_.is_null())
    return;
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_onAutomaticGenerationStatusChanged(
        base::android::AttachCurrentThread(), obj, available);
  }
}

void ManualFillingViewAndroid::OnFillingTriggered(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint tab_type,
    const base::android::JavaParamRef<jobject>& j_user_info_field) {
  controller_->OnFillingTriggered(
      static_cast<autofill::AccessoryTabType>(tab_type),
      ConvertJavaUserInfoField(env, j_user_info_field));
}

void ManualFillingViewAndroid::OnOptionSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint selected_action) {
  controller_->OnOptionSelected(
      static_cast<autofill::AccessoryAction>(selected_action));
}

void ManualFillingViewAndroid::OnToggleChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint selected_action,
    jboolean enabled) {
  controller_->OnToggleChanged(
      static_cast<autofill::AccessoryAction>(selected_action), enabled);
}

void ManualFillingViewAndroid::OnViewDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  java_object_internal_.Reset(nullptr);
}

ScopedJavaLocalRef<jobject>
ManualFillingViewAndroid::ConvertAccessorySheetDataToJavaObject(
    JNIEnv* env,
    const AccessorySheetData& tab_data) {
  DCHECK(java_object_internal_);
  ScopedJavaLocalRef<jobject> j_tab_data =
      Java_ManualFillingComponentBridge_createAccessorySheetData(
          env, static_cast<int>(tab_data.get_sheet_type()),
          ConvertUTF16ToJavaString(env, tab_data.title()),
          ConvertUTF16ToJavaString(env, tab_data.warning()));

  if (tab_data.option_toggle().has_value()) {
    autofill::OptionToggle toggle = tab_data.option_toggle().value();
    Java_ManualFillingComponentBridge_addOptionToggleToAccessorySheetData(
        env, java_object_internal_, j_tab_data,
        ConvertUTF16ToJavaString(env, toggle.display_text()),
        toggle.is_enabled(), static_cast<int>(toggle.accessory_action()));
  }

  for (const UserInfo& user_info : tab_data.user_info_list()) {
    ScopedJavaLocalRef<jobject> j_user_info =
        Java_ManualFillingComponentBridge_addUserInfoToAccessorySheetData(
            env, java_object_internal_, j_tab_data,
            ConvertUTF8ToJavaString(env, user_info.origin()),
            user_info.is_psl_match().value());
    for (const UserInfo::Field& field : user_info.fields()) {
      Java_ManualFillingComponentBridge_addFieldToUserInfo(
          env, java_object_internal_, j_user_info,
          static_cast<int>(tab_data.get_sheet_type()),
          ConvertUTF16ToJavaString(env, field.display_text()),
          ConvertUTF16ToJavaString(env, field.a11y_description()),
          ConvertUTF8ToJavaString(env, field.id()), field.is_obfuscated(),
          field.selectable());
    }
  }

  for (const FooterCommand& footer_command : tab_data.footer_commands()) {
    Java_ManualFillingComponentBridge_addFooterCommandToAccessorySheetData(
        env, java_object_internal_, j_tab_data,
        ConvertUTF16ToJavaString(env, footer_command.display_text()),
        static_cast<int>(footer_command.accessory_action()));
  }
  return j_tab_data;
}

UserInfo::Field ManualFillingViewAndroid::ConvertJavaUserInfoField(
    JNIEnv* env,
    const JavaRef<jobject>& j_field_to_convert) {
  std::u16string display_text = ConvertJavaStringToUTF16(
      env, Java_UserInfoField_getDisplayText(env, j_field_to_convert));
  std::u16string a11y_description = ConvertJavaStringToUTF16(
      env, Java_UserInfoField_getA11yDescription(env, j_field_to_convert));
  std::string id = ConvertJavaStringToUTF8(
      env, Java_UserInfoField_getId(env, j_field_to_convert));
  bool is_obfuscated = Java_UserInfoField_isObfuscated(env, j_field_to_convert);
  bool selectable = Java_UserInfoField_isSelectable(env, j_field_to_convert);
  return UserInfo::Field(display_text, a11y_description, id, is_obfuscated,
                         selectable);
}

base::android::ScopedJavaGlobalRef<jobject>
ManualFillingViewAndroid::GetOrCreateJavaObject() {
  if (java_object_internal_)
    return java_object_internal_;
  if (controller_->container_view() == nullptr ||
      controller_->container_view()->GetWindowAndroid() == nullptr) {
    return nullptr;  // No window attached (yet or anymore).
  }
  java_object_internal_.Reset(Java_ManualFillingComponentBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      controller_->container_view()->GetWindowAndroid()->GetJavaObject()));
  return java_object_internal_;
}

// static
void JNI_ManualFillingComponentBridge_CachePasswordSheetDataForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jobjectArray>& j_usernames,
    const base::android::JavaParamRef<jobjectArray>& j_passwords,
    jboolean j_blocklisted) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  url::Origin origin = url::Origin::Create(web_contents->GetLastCommittedURL());
  std::vector<std::string> usernames;
  std::vector<std::string> passwords;
  base::android::AppendJavaStringArrayToStringVector(env, j_usernames,
                                                     &usernames);
  base::android::AppendJavaStringArrayToStringVector(env, j_passwords,
                                                     &passwords);
  std::vector<password_manager::PasswordForm> password_forms(usernames.size());
  std::vector<const password_manager::PasswordForm*> credentials;
  for (unsigned int i = 0; i < usernames.size(); ++i) {
    password_forms[i].url = origin.GetURL();
    password_forms[i].username_value = base::ASCIIToUTF16(usernames[i]);
    password_forms[i].password_value = base::ASCIIToUTF16(passwords[i]);
    credentials.push_back(&password_forms[i]);
  }
  return ChromePasswordManagerClient::FromWebContents(web_contents)
      ->GetCredentialCacheForTesting()
      ->SaveCredentialsAndBlocklistedForOrigin(
          credentials,
          password_manager::CredentialCache::IsOriginBlocklisted(j_blocklisted),
          origin);
}

// static
void JNI_ManualFillingComponentBridge_NotifyFocusedFieldTypeForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jlong j_focused_field_id,
    jint j_available) {
  ManualFillingControllerImpl::GetOrCreate(
      content::WebContents::FromJavaWebContents(j_web_contents))
      ->NotifyFocusedInputChanged(
          autofill::FieldRendererId(j_focused_field_id),
          static_cast<autofill::mojom::FocusedFieldType>(j_available));
}

// static
void JNI_ManualFillingComponentBridge_SignalAutoGenerationStatusForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jboolean j_available) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  // Bypass the generation controller when sending this status to the UI to
  // avoid setup overhead, since its logic is currently not needed for tests.
  ManualFillingControllerImpl::GetOrCreate(web_contents)
      ->OnAutomaticGenerationStatusChanged(j_available);
}

// static
void JNI_ManualFillingComponentBridge_DisableServerPredictionsForTesting(
    JNIEnv* env) {
  password_manager::PasswordFormManager::
      DisableFillingServerPredictionsForTesting();
}

// static
std::unique_ptr<ManualFillingViewInterface> ManualFillingViewInterface::Create(
    ManualFillingController* controller) {
  return std::make_unique<ManualFillingViewAndroid>(controller);
}
