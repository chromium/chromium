// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_BASE_ACCOUNT_MANAGER_FACADE_ANDROID_H_
#define COMPONENTS_SIGNIN_INTERNAL_BASE_ACCOUNT_MANAGER_FACADE_ANDROID_H_

#include "base/android/scoped_java_ref.h"

// Simple accessor to java's AccountManagerFacade
class AccountManagerFacadeAndroid {
 public:
  // Returns a reference to the corresponding Java AccountManagerFacade object.
  // TODO(crbug.com/986435) Remove direct access to AccountManagerFacade.get
  // from C++ when IdentityServicesProvider will handle its instance management.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_BASE_ACCOUNT_MANAGER_FACADE_ANDROID_H_
