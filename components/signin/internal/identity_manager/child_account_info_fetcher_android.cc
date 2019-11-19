// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/child_account_info_fetcher_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "components/signin/core/browser/android/jni_headers/ChildAccountInfoFetcher_jni.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"

using base::android::JavaParamRef;

// static
std::unique_ptr<ChildAccountInfoFetcherAndroid>
ChildAccountInfoFetcherAndroid::Create(AccountFetcherService* service,
                                       const CoreAccountId& account_id) {
  std::string account_name =
      service->account_tracker_service()->GetAccountInfo(account_id).email;
  // The AccountTrackerService may not be populated correctly in tests.
  if (account_name.empty())
    return nullptr;

  // Call the constructor directly instead of using std::make_unique because the
  // constructor is private.
  return base::WrapUnique(
      new ChildAccountInfoFetcherAndroid(service, account_id, account_name));
}

void ChildAccountInfoFetcherAndroid::InitializeForTests() {
  Java_ChildAccountInfoFetcher_initializeForTests(
      base::android::AttachCurrentThread());
}

ChildAccountInfoFetcherAndroid::ChildAccountInfoFetcherAndroid(
    AccountFetcherService* service,
    const CoreAccountId& account_id,
    const std::string& account_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  j_child_account_info_fetcher_.Reset(Java_ChildAccountInfoFetcher_create(
      env, reinterpret_cast<jlong>(service),
      base::android::ConvertUTF8ToJavaString(env, account_id.id),
      base::android::ConvertUTF8ToJavaString(env, account_name)));
}

ChildAccountInfoFetcherAndroid::~ChildAccountInfoFetcherAndroid() {
  Java_ChildAccountInfoFetcher_destroy(base::android::AttachCurrentThread(),
                                       j_child_account_info_fetcher_);
}

void JNI_ChildAccountInfoFetcher_SetIsChildAccount(
    JNIEnv* env,
    jlong native_service,
    const JavaParamRef<jstring>& j_account_id,
    jboolean is_child_account) {
  AccountFetcherService* service =
      reinterpret_cast<AccountFetcherService*>(native_service);
  service->SetIsChildAccount(
      CoreAccountId(base::android::ConvertJavaStringToUTF8(env, j_account_id)),
      is_child_account);
}
