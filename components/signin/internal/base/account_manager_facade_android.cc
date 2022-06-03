// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/base/account_manager_facade_android.h"

#include "components/signin/public/android/jni_headers/AccountManagerFacadeProvider_jni.h"

base::android::ScopedJavaLocalRef<jobject>
AccountManagerFacadeAndroid::GetJavaObject() {
  return signin::Java_AccountManagerFacadeProvider_getInstance(
      base::android::AttachCurrentThread());
}
