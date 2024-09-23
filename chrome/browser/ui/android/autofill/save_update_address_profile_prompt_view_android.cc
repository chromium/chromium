// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_update_address_profile_prompt_view_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SaveUpdateAddressProfilePrompt_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

SaveUpdateAddressProfilePromptViewAndroid::
    SaveUpdateAddressProfilePromptViewAndroid(
        content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

SaveUpdateAddressProfilePromptViewAndroid::
    ~SaveUpdateAddressProfilePromptViewAndroid() {
  if (java_object_) {
    Java_SaveUpdateAddressProfilePrompt_dismiss(
        base::android::AttachCurrentThread(), java_object_);
  }
}

bool SaveUpdateAddressProfilePromptViewAndroid::Show(
    SaveUpdateAddressProfilePromptController* controller,
    const AutofillProfile& autofill_profile,
    bool is_update,
    bool is_migration_to_account) {
  DCHECK(controller);
  if (!web_contents_->GetTopLevelNativeWindow()) {
    return false;  // No window attached (yet or anymore).
  }

  base::android::ScopedJavaLocalRef<jobject> java_controller =
      controller->GetJavaObject();
  if (!java_controller)
    return false;

  Profile* browser_profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_autofill_profile =
      autofill_profile.CreateJavaObject(
          g_browser_process->GetApplicationLocale());
  java_object_.Reset(Java_SaveUpdateAddressProfilePrompt_create(
      env, web_contents_->GetTopLevelNativeWindow()->GetJavaObject(),
      java_controller, browser_profile->GetJavaObject(), java_autofill_profile,
      static_cast<jboolean>(is_update),
      static_cast<jboolean>(is_migration_to_account)));
  if (!java_object_)
    return false;

  SetContent(controller, IdentityManagerFactory::GetForProfile(browser_profile),
             is_update);
  Java_SaveUpdateAddressProfilePrompt_show(env, java_object_);
  return true;
}

void SaveUpdateAddressProfilePromptViewAndroid::SetContent(
    SaveUpdateAddressProfilePromptController* controller,
    signin::IdentityManager* identity_manager,
    bool is_update) {
  DCHECK(controller);
  DCHECK(java_object_);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> title =
      base::android::ConvertUTF16ToJavaString(env, controller->GetTitle());
  ScopedJavaLocalRef<jstring> record_type_notice =
      base::android::ConvertUTF16ToJavaString(
          env, controller->GetRecordTypeNotice(identity_manager));
  ScopedJavaLocalRef<jstring> positive_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, controller->GetPositiveButtonText());
  ScopedJavaLocalRef<jstring> negative_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, controller->GetNegativeButtonText());
  Java_SaveUpdateAddressProfilePrompt_setDialogDetails(
      env, java_object_, title, positive_button_text, negative_button_text);
  Java_SaveUpdateAddressProfilePrompt_setRecordTypeNotice(env, java_object_,
                                                          record_type_notice);

  if (is_update) {
    ScopedJavaLocalRef<jstring> subtitle =
        base::android::ConvertUTF16ToJavaString(env, controller->GetSubtitle());
    std::pair<std::u16string, std::u16string> differences =
        controller->GetDiffFromOldToNewProfile();
    ScopedJavaLocalRef<jstring> old_details =
        base::android::ConvertUTF16ToJavaString(env, differences.first);
    ScopedJavaLocalRef<jstring> new_details =
        base::android::ConvertUTF16ToJavaString(env, differences.second);
    Java_SaveUpdateAddressProfilePrompt_setUpdateDetails(
        env, java_object_, subtitle, old_details, new_details);
  } else {
    ScopedJavaLocalRef<jstring> address =
        base::android::ConvertUTF16ToJavaString(env, controller->GetAddress());
    ScopedJavaLocalRef<jstring> email =
        base::android::ConvertUTF16ToJavaString(env, controller->GetEmail());
    ScopedJavaLocalRef<jstring> phone = base::android::ConvertUTF16ToJavaString(
        env, controller->GetPhoneNumber());
    Java_SaveUpdateAddressProfilePrompt_setSaveOrMigrateDetails(
        env, java_object_, address, email, phone);
  }
}

}  // namespace autofill
