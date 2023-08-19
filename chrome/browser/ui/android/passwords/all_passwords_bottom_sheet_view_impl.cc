// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/features/keyboard_accessory/internal/jni/AllPasswordsBottomSheetBridge_jni.h"
#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

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

  Java_AllPasswordsBottomSheetBridge_createCredentialArray(env, java_object,
                                                           credentials.size());

  int index = 0;
  for (const auto& credential : credentials) {
    auto facet = password_manager::FacetURI::FromPotentiallyInvalidSpec(
        credential->signon_realm);
    std::string app_display_name = credential->app_display_name;
    if (facet.IsValidAndroidFacetURI() && app_display_name.empty()) {
      app_display_name = l10n_util::GetStringFUTF8(
          IDS_SETTINGS_PASSWORDS_ANDROID_APP,
          base::UTF8ToUTF16(facet.android_package_name()));
    }

    Java_AllPasswordsBottomSheetBridge_insertCredential(
        env, java_object, index++,
        ConvertUTF16ToJavaString(env, credential->username_value),
        ConvertUTF16ToJavaString(env, credential->password_value),
        ConvertUTF16ToJavaString(env, GetDisplayUsername(*credential)),
        ConvertUTF8ToJavaString(env, credential->url.spec()),
        facet.IsValidAndroidFacetURI(),
        ConvertUTF8ToJavaString(env, app_display_name));
  }

  const bool is_password_field =
      focused_field_type == FocusedFieldType::kFillablePasswordField;
  Java_AllPasswordsBottomSheetBridge_showCredentials(env, java_object,
                                                     is_password_field);
}

void AllPasswordsBottomSheetViewImpl::OnCredentialSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& username,
    const base::android::JavaParamRef<jstring>& password,
    jboolean requests_to_fill_password) {
  controller_->OnCredentialSelected(
      ConvertJavaStringToUTF16(env, username),
      ConvertJavaStringToUTF16(env, password),
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
             controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
             ConvertUTF8ToJavaString(AttachCurrentThread(),
                                     controller_->GetFrameUrl().spec()));
}
