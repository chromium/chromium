// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_android.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/signin/public/android/jni_headers/AccountCapabilitiesFetcher_jni.h"

AccountCapabilitiesFetcherAndroid::AccountCapabilitiesFetcherAndroid(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnSomeCapabilitiesFetchedCallback
        on_some_capabilities_fetched_callback,
    AccountCapabilitiesFetcher::OnAllFetchesCompleteCallback
        on_all_fetches_complete_callback)
    : AccountCapabilitiesFetcher(
          account_info,
          fetch_priority,
          std::move(on_some_capabilities_fetched_callback),
          std::move(on_all_fetches_complete_callback)) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      signin::Java_AccountCapabilitiesFetcher_Constructor(
          env, account_info,
          base::BindOnce(
              &AccountCapabilitiesFetcherAndroid::OnCapabilitiesFetchComplete,
              weak_ptr_factory_.GetWeakPtr()));
  java_ref_.Reset(env, local_java_ref);
}

AccountCapabilitiesFetcherAndroid::~AccountCapabilitiesFetcherAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  signin::Java_AccountCapabilitiesFetcher_destroy(env, java_ref_);
}

void AccountCapabilitiesFetcherAndroid::StartImpl() {
  JNIEnv* env = base::android::AttachCurrentThread();
  signin::Java_AccountCapabilitiesFetcher_startFetchingAccountCapabilities(
      env, java_ref_);
}

void AccountCapabilitiesFetcherAndroid::OnCapabilitiesFetchComplete(
    const base::android::JavaRef<jobject>& account_capabilities) {
  JNIEnv* env = base::android::AttachCurrentThread();
  UpdateAndCompleteFetchAndMaybeDestroySelf(
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, account_capabilities));
}
