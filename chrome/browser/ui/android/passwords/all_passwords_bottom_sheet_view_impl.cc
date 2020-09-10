// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/features/keyboard_accessory/jni_headers/AllPasswordsBottomSheetBridge_jni.h"
#include "chrome/android/features/keyboard_accessory/jni_headers/Credential_jni.h"
#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "ui/android/window_android.h"

using autofill::mojom::FocusedFieldType;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using password_manager::UiCredential;

namespace {

UiCredential ConvertJavaCredential(JNIEnv* env,
                                   const JavaParamRef<jobject>& credential) {
  return UiCredential(
      ConvertJavaStringToUTF16(env,
                               Java_Credential_getUsername(env, credential)),
      ConvertJavaStringToUTF16(env,
                               Java_Credential_getPassword(env, credential)),
      url::Origin::Create(GURL(ConvertJavaStringToUTF8(
          env, Java_Credential_getOriginUrl(env, credential)))),
      UiCredential::IsPublicSuffixMatch(
          Java_Credential_isPublicSuffixMatch(env, credential)),
      UiCredential::IsAffiliationBasedMatch(
          Java_Credential_isAffiliationBasedMatch(env, credential)));
}

}  // namespace

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
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& credentials,
    FocusedFieldType focused_field_type) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;

  JNIEnv* env = AttachCurrentThread();

  Java_AllPasswordsBottomSheetBridge_createCredentialArray(env, java_object,
                                                           credentials.size());

  int index = 0;
  for (const auto& credential : credentials) {
    Java_AllPasswordsBottomSheetBridge_insertCredential(
        env, java_object, index++,
        ConvertUTF16ToJavaString(env, credential->username_value),
        ConvertUTF16ToJavaString(env, credential->password_value),
        ConvertUTF16ToJavaString(env, GetDisplayUsername(*credential)),
        ConvertUTF8ToJavaString(env, credential->url.spec()),
        credential->is_public_suffix_match,
        credential->is_affiliation_based_match);
  }

  const bool is_password_field =
      focused_field_type == FocusedFieldType::kFillablePasswordField;
  Java_AllPasswordsBottomSheetBridge_showCredentials(env, java_object,
                                                     is_password_field);
}

void AllPasswordsBottomSheetViewImpl::OnCredentialSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& credential) {
  controller_->OnCredentialSelected(ConvertJavaCredential(env, credential));
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
             controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject());
}
