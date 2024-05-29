// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/child_account_info_fetcher_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/public/identity_manager/account_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/signin/public/android/jni_headers/ChildAccountInfoFetcher_jni.h"

using base::android::JavaParamRef;

// static
std::unique_ptr<ChildAccountInfoFetcherAndroid>
ChildAccountInfoFetcherAndroid::Create(AccountFetcherService* service,
                                       const CoreAccountId& account_id) {
  CoreAccountInfo account_info =
      service->account_tracker_service()->GetAccountInfo(account_id);
  // The AccountTrackerService may not be populated correctly in tests.
  if (account_info.email.empty())
    return nullptr;

  // Call the constructor directly instead of using std::make_unique because the
  // constructor is private.
  return base::WrapUnique(
      new ChildAccountInfoFetcherAndroid(service, account_info));
}

ChildAccountInfoFetcherAndroid::ChildAccountInfoFetcherAndroid(
    AccountFetcherService* service,
    const CoreAccountInfo& account_info) {
  JNIEnv* env = base::android::AttachCurrentThread();
  j_child_account_info_fetcher_.Reset(
      signin::Java_ChildAccountInfoFetcher_create(
          env, reinterpret_cast<jlong>(service),
          ConvertToJavaCoreAccountInfo(env, account_info)));
}

ChildAccountInfoFetcherAndroid::~ChildAccountInfoFetcherAndroid() {
  signin::Java_ChildAccountInfoFetcher_destroy(
      base::android::AttachCurrentThread(), j_child_account_info_fetcher_);
}

void signin::JNI_ChildAccountInfoFetcher_SetIsChildAccount(
    JNIEnv* env,
    jlong native_service,
    const JavaParamRef<jobject>& j_account_id,
    jboolean is_child_account) {
  AccountFetcherService* service =
      reinterpret_cast<AccountFetcherService*>(native_service);
  service->SetIsChildAccount(ConvertFromJavaCoreAccountId(env, j_account_id),
                             is_child_account);
}
