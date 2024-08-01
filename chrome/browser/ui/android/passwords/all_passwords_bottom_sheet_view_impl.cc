// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view_impl.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/features/keyboard_accessory/internal/jni/AllPasswordsBottomSheetBridge_jni.h"
#include "chrome/android/features/keyboard_accessory/internal/jni/Credential_jni.h"

using autofill::mojom::FocusedFieldType;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;


AllPasswordsBottomSheetViewImpl::AllPasswordsBottomSheetViewImpl(
    AllPasswordsBottomSheetController* controller)
    : controller_(controller) {}

AllPasswordsBottomSheetViewImpl::~AllPasswordsBottomSheetViewImpl() {
  if (java_object_internal_) {
    // Don't create an object just for destruction.
    Java_AllPasswordsBottomSheetBridge_destroy(AttachCurrentThread(),
                                               java_object_internal_);
  }
}

void AllPasswordsBottomSheetViewImpl::Show(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
        credentials,
    FocusedFieldType focused_field_type) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;

  JNIEnv* env = AttachCurrentThread();

  std::vector<base::android::ScopedJavaLocalRef<jobject>> java_credentials;
  java_credentials.reserve(credentials.size());
  for (const auto& credential : credentials) {
    auto facet = affiliations::FacetURI::FromPotentiallyInvalidSpec(
        credential->signon_realm);
    std::string app_display_name = credential->app_display_name;
    if (facet.IsValidAndroidFacetURI() && app_display_name.empty()) {
      app_display_name = l10n_util::GetStringFUTF8(
          IDS_SETTINGS_PASSWORDS_ANDROID_APP,
          base::UTF8ToUTF16(facet.android_package_name()));
    }

    java_credentials.emplace_back(Java_Credential_Constructor(
        env, credential->username_value, credential->password_value,
        GetDisplayUsername(*credential), credential->url.spec(),
        facet.IsValidAndroidFacetURI(), app_display_name,
        controller_->IsPlusAddress(
            base::UTF16ToUTF8(credential->username_value))));
  }

  const bool is_password_field =
      focused_field_type == FocusedFieldType::kFillablePasswordField;
  Java_AllPasswordsBottomSheetBridge_showCredentials(
      env, java_object, java_credentials, is_password_field);
}

void AllPasswordsBottomSheetViewImpl::OnCredentialSelected(
    JNIEnv* env,
    std::u16string& username,
    std::u16string& password,
    jboolean requests_to_fill_password) {
  controller_->OnCredentialSelected(
      username, password,
      AllPasswordsBottomSheetController::RequestsToFillPassword(
          requests_to_fill_password));
}

void AllPasswordsBottomSheetViewImpl::OnDismiss(JNIEnv* env) {
  controller_->OnDismiss();
}

base::android::ScopedJavaGlobalRef<jobject>
AllPasswordsBottomSheetViewImpl::GetOrCreateJavaObject() {
  if (java_object_internal_) {
    return java_object_internal_;
  }
  if (controller_->GetNativeView() == nullptr ||
      controller_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return nullptr;  // No window attached (yet or anymore).
  }
  return java_object_internal_ = Java_AllPasswordsBottomSheetBridge_create(
             AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
             controller_->GetProfile()->GetJavaObject(),
             controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
             controller_->GetFrameUrl().spec());
}
