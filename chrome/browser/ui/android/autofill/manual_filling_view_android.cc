// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/manual_filling_view_android.h"

#include <jni.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/features/keyboard_accessory/internal/jni/ManualFillingComponentBridge_jni.h"
#include "chrome/android/features/keyboard_accessory/public/jni/UserInfoField_jni.h"

using autofill::AccessorySheetData;
using autofill::AccessorySheetField;
using autofill::FooterCommand;
using autofill::UserInfo;
using autofill::password_generation::PasswordGenerationUIData;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using password_manager::PasswordForm;

namespace {

AccessorySheetField ConvertJavaUserInfoField(
    JNIEnv* env,
    const JavaRef<jobject>& j_field_to_convert) {
  std::u16string display_text = ConvertJavaStringToUTF16(
      env, Java_UserInfoField_getDisplayText(env, j_field_to_convert));
  std::u16string text_to_fill = ConvertJavaStringToUTF16(
      env, Java_UserInfoField_getTextToFill(env, j_field_to_convert));
  std::u16string a11y_description = ConvertJavaStringToUTF16(
      env, Java_UserInfoField_getA11yDescription(env, j_field_to_convert));
  std::string id = ConvertJavaStringToUTF8(
      env, Java_UserInfoField_getId(env, j_field_to_convert));
  bool is_obfuscated = Java_UserInfoField_isObfuscated(env, j_field_to_convert);
  bool selectable = Java_UserInfoField_isSelectable(env, j_field_to_convert);
  return AccessorySheetField::Builder()
      .SetDisplayText(std::move(display_text))
      .SetTextToFill(std::move(text_to_fill))
      .SetA11yDescription(std::move(a11y_description))
      .SetId(std::move(id))
      .SetIsObfuscated(is_obfuscated)
      .SetSelectable(selectable)
      .Build();
}

// The Conversion does not require any actual methods from either side of the
// bridge — it's only required because it is referenced in callbacks. Therefore,
// the java_object can always be used, even if the controller has been
// dismissed.
// TODO(crbug.com/40858913): Pass a delegate/callback and not the bridge object.
ScopedJavaGlobalRef<jobject> ConvertAccessorySheetDataToJavaObject(
    ScopedJavaGlobalRef<jobject> java_object,
    AccessorySheetData tab_data) {
  // Keep the ManualFillingViewAndroid:: prefix for easier trace comparison.
  TRACE_EVENT0(
      "passwords",
      "ManualFillingViewAndroid::ConvertAccessorySheetDataToJavaObject");
  DCHECK(java_object);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaGlobalRef<jobject> j_tab_data;
  j_tab_data.Reset(Java_ManualFillingComponentBridge_createAccessorySheetData(
      env, static_cast<int>(tab_data.get_sheet_type()),
      tab_data.user_info_title(), tab_data.plus_address_title(),
      tab_data.warning()));

  if (tab_data.option_toggle().has_value()) {
    const autofill::OptionToggle& toggle = tab_data.option_toggle().value();
    Java_ManualFillingComponentBridge_addOptionToggleToAccessorySheetData(
        env, java_object, j_tab_data, toggle.display_text(),
        toggle.is_enabled(), static_cast<int>(toggle.accessory_action()));
  }

  for (const autofill::PlusAddressInfo& plus_address_info :
       tab_data.plus_address_info_list()) {
    Java_ManualFillingComponentBridge_addPlusAddressInfoToAccessorySheetData(
        env, java_object, j_tab_data,
        static_cast<int>(tab_data.get_sheet_type()), plus_address_info.origin(),
        plus_address_info.plus_address().display_text());
  }

  for (const autofill::PasskeySection& passkey_section :
       tab_data.passkey_section_list()) {
    Java_ManualFillingComponentBridge_addPasskeySectionToAccessorySheetData(
        env, java_object, j_tab_data,
        static_cast<int>(tab_data.get_sheet_type()),
        passkey_section.display_name(), passkey_section.passkey_id());
  }

  for (const UserInfo& user_info : tab_data.user_info_list()) {
    ScopedJavaLocalRef<jobject> j_user_info =
        Java_ManualFillingComponentBridge_addUserInfoToAccessorySheetData(
            env, java_object, j_tab_data, user_info.origin(),
            user_info.is_exact_match().value(),
            url::GURLAndroid::FromNativeGURL(env, user_info.icon_url()));
    for (const AccessorySheetField& field : user_info.fields()) {
      Java_ManualFillingComponentBridge_addFieldToUserInfo(
          env, java_object, j_user_info,
          static_cast<int>(tab_data.get_sheet_type()), field.display_text(),
          field.text_to_fill(), field.a11y_description(), field.id(),
          field.icon_id(), field.is_obfuscated(), field.selectable());
    }
  }

  for (const autofill::PromoCodeInfo& promo_code_info :
       tab_data.promo_code_info_list()) {
    const AccessorySheetField& promo_code = promo_code_info.promo_code();
    const std::u16string& detailsText = promo_code_info.details_text();
    Java_ManualFillingComponentBridge_addPromoCodeInfoToAccessorySheetData(
        env, java_object, j_tab_data,
        static_cast<int>(tab_data.get_sheet_type()), promo_code.display_text(),
        promo_code.text_to_fill(), promo_code.a11y_description(),
        promo_code.id(), promo_code.is_obfuscated(), detailsText);
  }

  for (const autofill::IbanInfo& iban_info : tab_data.iban_info_list()) {
    const AccessorySheetField& value = iban_info.value();
    Java_ManualFillingComponentBridge_addIbanInfoToAccessorySheetData(
        env, java_object, j_tab_data,
        static_cast<int>(tab_data.get_sheet_type()), value.id(),
        value.display_text(), value.text_to_fill());
  }

  for (const FooterCommand& footer_command : tab_data.footer_commands()) {
    Java_ManualFillingComponentBridge_addFooterCommandToAccessorySheetData(
        env, java_object, j_tab_data, footer_command.display_text(),
        static_cast<int>(footer_command.accessory_action()));
  }
  return j_tab_data;
}

}  // namespace

ManualFillingViewAndroid::ManualFillingViewAndroid(
    ManualFillingController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {}

ManualFillingViewAndroid::~ManualFillingViewAndroid() {
  if (!java_object_internal_) {
    return;  // No work to do.
  }
  Java_ManualFillingComponentBridge_destroy(
      base::android::AttachCurrentThread(), java_object_internal_);
  java_object_internal_.Reset(nullptr);
}

void ManualFillingViewAndroid::OnItemsAvailable(AccessorySheetData data) {
  TRACE_EVENT0("passwords", "ManualFillingViewAndroid::OnItemsAvailable");
  if (auto obj = GetOrCreateJavaObject()) {
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&ConvertAccessorySheetDataToJavaObject, obj,
                       std::move(data)),
        base::BindOnce(&Java_ManualFillingComponentBridge_onItemsAvailable,
                       base::android::AttachCurrentThread(), obj));
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

void ManualFillingViewAndroid::Show(WaitForKeyboard wait_for_keyboard) {
  TRACE_EVENT0("passwords", "ManualFillingViewAndroid::Show");
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_show(base::android::AttachCurrentThread(),
                                           obj, wait_for_keyboard.value());
  }
}

void ManualFillingViewAndroid::Hide() {
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_hide(base::android::AttachCurrentThread(),
                                           obj);
  }
}

void ManualFillingViewAndroid::ShowAccessorySheetTab(
    const autofill::AccessoryTabType& tab_type) {
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_showAccessorySheetTab(
        base::android::AttachCurrentThread(), obj, static_cast<int>(tab_type));
  }
}
void ManualFillingViewAndroid::OnAccessoryActionAvailabilityChanged(
    ShouldShowAction shouldShowAction,
    autofill::AccessoryAction action) {
  if (!shouldShowAction && java_object_internal_.is_null()) {
    return;
  }
  if (auto obj = GetOrCreateJavaObject()) {
    Java_ManualFillingComponentBridge_onAccessoryActionAvailabilityChanged(
        base::android::AttachCurrentThread(), obj, shouldShowAction.value(),
        static_cast<int>(action));
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

void ManualFillingViewAndroid::OnPasskeySelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint tab_type,
    std::vector<uint8_t>& passkey) {
  controller_->OnPasskeySelected(
      static_cast<autofill::AccessoryTabType>(tab_type), passkey);
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

void ManualFillingViewAndroid::RequestAccessorySheet(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint tab_type) {
  // controller_ owns this class. Therefore, the callback can't outlive the view
  // and base::Unretained is always a valid reference.
  controller_->RequestAccessorySheet(
      static_cast<autofill::AccessoryTabType>(tab_type),
      base::BindOnce(&ManualFillingViewAndroid::OnItemsAvailable,
                     base::Unretained(this)));
}

void ManualFillingViewAndroid::OnViewDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  java_object_internal_.Reset(nullptr);
}

base::android::ScopedJavaGlobalRef<jobject>
ManualFillingViewAndroid::GetOrCreateJavaObject() {
  if (java_object_internal_) {
    return java_object_internal_;
  }
  if (controller_->container_view() == nullptr ||
      controller_->container_view()->GetWindowAndroid() == nullptr) {
    return nullptr;  // No window attached (yet or anymore).
  }
  java_object_internal_.Reset(Java_ManualFillingComponentBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      controller_->container_view()->GetWindowAndroid()->GetJavaObject(),
      web_contents_->GetJavaWebContents()));
  return java_object_internal_;
}

// static
void JNI_ManualFillingComponentBridge_CachePasswordSheetDataForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    std::vector<std::string>& usernames,
    std::vector<std::string>& passwords,
    jboolean j_blocklisted) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  url::Origin origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  std::vector<password_manager::PasswordForm> credentials(usernames.size());
  for (unsigned int i = 0; i < usernames.size(); ++i) {
    credentials[i].url = origin.GetURL();
    credentials[i].username_value = base::ASCIIToUTF16(usernames[i]);
    credentials[i].password_value = base::ASCIIToUTF16(passwords[i]);
    credentials[i].match_type =
        password_manager::PasswordForm::MatchType::kExact;
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
      ->OnAccessoryActionAvailabilityChanged(
          ManualFillingController::ShouldShowAction(j_available),
          autofill::AccessoryAction::GENERATE_PASSWORD_AUTOMATIC);
}

// static
void JNI_ManualFillingComponentBridge_DisableServerPredictionsForTesting(
    JNIEnv* env) {
  password_manager::PasswordFormManager::
      DisableFillingServerPredictionsForTesting();
}

// static
std::unique_ptr<ManualFillingViewInterface> ManualFillingViewInterface::Create(
    ManualFillingController* controller,
    content::WebContents* web_contents) {
  return std::make_unique<ManualFillingViewAndroid>(controller, web_contents);
}
