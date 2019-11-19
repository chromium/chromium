// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_CHILD_ACCOUNT_INFO_FETCHER_ANDROID_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_CHILD_ACCOUNT_INFO_FETCHER_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "google_apis/gaia/core_account_id.h"

class AccountFetcherService;

class ChildAccountInfoFetcherAndroid {
 public:
  static std::unique_ptr<ChildAccountInfoFetcherAndroid> Create(
      AccountFetcherService* service,
      const CoreAccountId& account_id);
  ~ChildAccountInfoFetcherAndroid();

  static void InitializeForTests();

 private:
  ChildAccountInfoFetcherAndroid(AccountFetcherService* service,
                                 const CoreAccountId& account_id,
                                 const std::string& account_name);

  base::android::ScopedJavaGlobalRef<jobject> j_child_account_info_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ChildAccountInfoFetcherAndroid);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_CHILD_ACCOUNT_INFO_FETCHER_ANDROID_H_
