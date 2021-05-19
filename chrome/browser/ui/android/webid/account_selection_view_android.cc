// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/webid/account_selection_view_android.h"

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/webid/jni_headers/AccountSelectionBridge_jni.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace {

std::vector<std::string> ConvertAccountToFields(const Account& account,
                                                const GURL& idp_url) {
  return {account.sub,        account.email,   account.name,
          account.given_name, account.picture, idp_url.spec()};
}

Account ConvertFieldsToAccount(JNIEnv* env,
                               const JavaParamRef<jobjectArray>& fields_obj) {
  std::vector<std::string> fields;
  AppendJavaStringArrayToStringVector(env, fields_obj, &fields);
  auto sub = fields[0];
  auto email = fields[1];
  auto name = fields[2];
  auto given_name = fields[3];
  auto picture = fields[4];
  return Account(sub, email, name, given_name, picture);
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

void AccountSelectionViewAndroid::Show(const GURL& rp_url,
                                       const GURL& idp_url,
                                       base::span<const Account> accounts) {
  if (!RecreateJavaObject()) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a waiting
    // state so report that AccountSelectionView is dismissed instead.
    delegate_->OnDismiss();
    return;
  }

  // Serialize the |accounts| span into a Java array and instruct the bridge
  // to show it together with |url| to the user.
  std::vector<std::vector<std::string>> accounts_fields(accounts.size());
  for (size_t i = 0; i < accounts.size(); ++i) {
    accounts_fields[i] = ConvertAccountToFields(accounts[i], idp_url);
  }

  JNIEnv* env = AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobjectArray> accounts_fields_obj =
      base::android::ToJavaArrayOfStringArray(env, accounts_fields);

  Java_AccountSelectionBridge_showAccounts(
      env, java_object_internal_, ConvertUTF8ToJavaString(env, rp_url.spec()),
      accounts_fields_obj);
}

void AccountSelectionViewAndroid::OnAccountSelected(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& account_fields_obj) {
  delegate_->OnAccountSelected(ConvertFieldsToAccount(env, account_fields_obj));
}

void AccountSelectionViewAndroid::OnDismiss(JNIEnv* env) {
  delegate_->OnDismiss();
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
