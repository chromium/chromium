// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_android.h"

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/signin/public/android/jni_headers/AccountCapabilitiesFetcher_jni.h"

namespace {
using OnAccountCapabilitiesFetchedCallback =
    base::OnceCallback<void(const std::optional<AccountCapabilities>&)>;
}

AccountCapabilitiesFetcherAndroid::AccountCapabilitiesFetcherAndroid(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback)
    : AccountCapabilitiesFetcher(account_info,
                                 fetch_priority,
                                 std::move(on_complete_callback)) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // This callback is "owned" by the Java counterpart. There is no explicit way
  // of passing object ownership to Java, so allocate the callback on heap and
  // pass a raw pointer to Java. The callback object will be collected once
  // Java calls back to native in
  // `JNI_AccountCapabilitiesFetcher_setAccountCapabilities()`.
  auto heap_callback =
      std::make_unique<OnAccountCapabilitiesFetchedCallback>(base::BindOnce(
          &AccountCapabilitiesFetcherAndroid::CompleteFetchAndMaybeDestroySelf,
          weak_ptr_factory_.GetWeakPtr()));
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      signin::Java_AccountCapabilitiesFetcher_Constructor(
          env, ConvertToJavaCoreAccountInfo(env, account_info),
          reinterpret_cast<intptr_t>(heap_callback.release()));
  java_ref_.Reset(env, local_java_ref.obj());
}

AccountCapabilitiesFetcherAndroid::~AccountCapabilitiesFetcherAndroid() =
    default;

void AccountCapabilitiesFetcherAndroid::StartImpl() {
  JNIEnv* env = base::android::AttachCurrentThread();
  signin::Java_AccountCapabilitiesFetcher_startFetchingAccountCapabilities(
      env, java_ref_);
}

namespace signin {
void JNI_AccountCapabilitiesFetcher_OnCapabilitiesFetchComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& account_capabilities,
    jlong native_callback) {
  std::unique_ptr<OnAccountCapabilitiesFetchedCallback> heap_callback(
      reinterpret_cast<OnAccountCapabilitiesFetchedCallback*>(native_callback));
  std::move(*heap_callback)
      .Run(AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, account_capabilities));
}
}  // namespace signin
