// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/webid/account_selection_view_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/webid/jni_headers/AccountSelectionBridge_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/Account_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/ClientIdMetadata_jni.h"
#include "chrome/browser/ui/android/webid/jni_headers/IdentityProviderMetadata_jni.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "ui/android/color_utils_android.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

ScopedJavaLocalRef<jobject> ConvertToJavaAccount(JNIEnv* env,
                                                 const Account& account) {
  return Java_Account_Constructor(
      env, ConvertUTF8ToJavaString(env, account.id),
      ConvertUTF8ToJavaString(env, account.email),
      ConvertUTF8ToJavaString(env, account.name),
      ConvertUTF8ToJavaString(env, account.given_name),
      url::GURLAndroid::FromNativeGURL(env, account.picture),
      account.login_state == Account::LoginState::kSignIn);
}

ScopedJavaLocalRef<jobject> ConvertToJavaIdentityProviderMetadata(
    JNIEnv* env,
    const content::IdentityProviderMetadata& metadata) {
  base::android::ScopedJavaLocalRef<jobject> java_brand_icon;
  if (!metadata.brand_icon.isNull())
    java_brand_icon = gfx::ConvertToJavaBitmap(metadata.brand_icon);
  return Java_IdentityProviderMetadata_Constructor(
      env, ui::OptionalSkColorToJavaColor(metadata.brand_text_color),
      ui::OptionalSkColorToJavaColor(metadata.brand_background_color),
      java_brand_icon);
}

ScopedJavaLocalRef<jobject> ConvertToJavaClientIdMetadata(
    JNIEnv* env,
    const content::ClientIdData& data) {
  return Java_ClientIdMetadata_Constructor(
      env, url::GURLAndroid::FromNativeGURL(env, data.terms_of_service_url),
      url::GURLAndroid::FromNativeGURL(env, data.privacy_policy_url));
}

ScopedJavaLocalRef<jobjectArray> ConvertToJavaAccounts(
    JNIEnv* env,
    base::span<const Account> accounts) {
  ScopedJavaLocalRef<jclass> account_clazz = base::android::GetClass(
      env, "org/chromium/chrome/browser/ui/android/webid/data/Account");
  ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(accounts.size(), account_clazz.obj(), nullptr));

  base::android::CheckException(env);

  for (size_t i = 0; i < accounts.size(); ++i) {
    ScopedJavaLocalRef<jobject> item = ConvertToJavaAccount(env, accounts[i]);
    env->SetObjectArrayElement(array.obj(), i, item.obj());
  }
  return array;
}

Account ConvertFieldsToAccount(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& string_fields_obj,
    const JavaParamRef<jobject>& picture_url_obj,
    bool is_sign_in) {
  std::vector<std::string> string_fields;
  AppendJavaStringArrayToStringVector(env, string_fields_obj, &string_fields);
  auto account_id = string_fields[0];
  auto email = string_fields[1];
  auto name = string_fields[2];
  auto given_name = string_fields[3];

  Account::LoginState login_state =
      is_sign_in ? Account::LoginState::kSignIn : Account::LoginState::kSignUp;

  GURL picture_url = *url::GURLAndroid::ToNativeGURL(env, picture_url_obj);
  return Account(account_id, email, name, given_name, picture_url, login_state);
}

}  // namespace

AccountSelectionViewAndroid::AccountSelectionViewAndroid(
    AccountSelectionView::Delegate* delegate)
    : AccountSelectionView(delegate) {}

AccountSelectionViewAndroid::~AccountSelectionViewAndroid() {
  if (java_object_internal_) {
    // Don't create an object just for destruction.
    Java_AccountSelectionBridge_destroy(AttachCurrentThread(),
                                        java_object_internal_);
  }
}

void AccountSelectionViewAndroid::Show(
    const std::string& rp_etld_plus_one,
    const std::string& idp_etld_plus_one,
    base::span<const Account> accounts,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientIdData& client_data,
    Account::SignInMode sign_in_mode) {
  if (!RecreateJavaObject()) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a
    // waiting state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss(/* should_embargo=*/false);
    return;
  }

  // Serialize the |accounts| span into a Java array and instruct the bridge
  // to show it together with |url| to the user.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> accounts_obj =
      ConvertToJavaAccounts(env, accounts);
  ScopedJavaLocalRef<jobject> idp_metadata_obj =
      ConvertToJavaIdentityProviderMetadata(env, idp_metadata);
  ScopedJavaLocalRef<jobject> client_id_metadata_obj =
      ConvertToJavaClientIdMetadata(env, client_data);
  Java_AccountSelectionBridge_showAccounts(
      env, java_object_internal_,
      ConvertUTF8ToJavaString(env, rp_etld_plus_one),
      ConvertUTF8ToJavaString(env, idp_etld_plus_one), accounts_obj,
      idp_metadata_obj, client_id_metadata_obj,
      sign_in_mode == Account::SignInMode::kAuto);
}

void AccountSelectionViewAndroid::OnAccountSelected(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& account_string_fields,
    const JavaParamRef<jobject>& account_picture_url,
    bool is_sign_in) {
  delegate_->OnAccountSelected(ConvertFieldsToAccount(
      env, account_string_fields, account_picture_url, is_sign_in));
}

void AccountSelectionViewAndroid::OnDismiss(JNIEnv* env, bool should_embargo) {
  delegate_->OnDismiss(should_embargo);
}

void AccountSelectionViewAndroid::OnAutoSignInCancelled(JNIEnv* env) {
  // TODO(yigu): Alternatively we could fall back to manual sign in flow.
  delegate_->OnDismiss(/*should_embargo=*/false);
}

bool AccountSelectionViewAndroid::RecreateJavaObject() {
  if (delegate_->GetNativeView() == nullptr ||
      delegate_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return false;  // No window attached (yet or anymore).
  }
  if (java_object_internal_) {
    Java_AccountSelectionBridge_destroy(AttachCurrentThread(),
                                        java_object_internal_);
  }
  java_object_internal_ = Java_AccountSelectionBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      delegate_->GetNativeView()->GetWindowAndroid()->GetJavaObject());
  return !!java_object_internal_;
}

// static
std::unique_ptr<AccountSelectionView> AccountSelectionView::Create(
    AccountSelectionView::Delegate* delegate) {
  return std::make_unique<AccountSelectionViewAndroid>(delegate);
}

// static
int AccountSelectionView::GetBrandIconMinimumSize() {
  return Java_AccountSelectionBridge_getBrandIconMinimumSize(
      base::android::AttachCurrentThread());
}

// static
int AccountSelectionView::GetBrandIconIdealSize() {
  return Java_AccountSelectionBridge_getBrandIconIdealSize(
      base::android::AttachCurrentThread());
}
