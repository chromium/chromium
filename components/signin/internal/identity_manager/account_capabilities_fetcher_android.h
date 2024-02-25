// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_ANDROID_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

// Android implementation of `AccountCapabilitiesFetcher`. It has a Java
// counterpart located in AccountCapabilitiesFetcher.java.
class AccountCapabilitiesFetcherAndroid : public AccountCapabilitiesFetcher {
 public:
  AccountCapabilitiesFetcherAndroid(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback);
  ~AccountCapabilitiesFetcherAndroid() override;

  AccountCapabilitiesFetcherAndroid(const AccountCapabilitiesFetcherAndroid&) =
      delete;
  AccountCapabilitiesFetcherAndroid& operator=(
      const AccountCapabilitiesFetcherAndroid&) = delete;

 protected:
  // AccountCapabilitiesFetcher:
  void StartImpl() override;

 private:
  // A reference to the Java counterpart of this object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  base::WeakPtrFactory<AccountCapabilitiesFetcherAndroid> weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_ANDROID_H_
