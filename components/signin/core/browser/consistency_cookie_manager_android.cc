// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager_android.h"

#include "components/signin/core/browser/android/jni_headers/ConsistencyCookieManager_jni.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin {

ConsistencyCookieManagerAndroid::ConsistencyCookieManagerAndroid(
    IdentityManager* identity_manager,
    SigninClient* signin_client,
    AccountReconcilor* reconcilor)
    : ConsistencyCookieManagerBase(signin_client, reconcilor) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_ref =
      Java_ConsistencyCookieManager_create(
          env, reinterpret_cast<intptr_t>(this),
          identity_manager->LegacyGetAccountTrackerServiceJavaObject());
  java_ref_.Reset(env, java_ref.obj());
  is_update_pending_in_java_ =
      Java_ConsistencyCookieManager_getIsUpdatePending(env, java_ref_);

  UpdateCookie();
}

ConsistencyCookieManagerAndroid::~ConsistencyCookieManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ConsistencyCookieManager_destroy(env, java_ref_);
}

void ConsistencyCookieManagerAndroid::OnIsUpdatePendingChanged(JNIEnv* env) {
  bool is_update_pending_in_java =
      Java_ConsistencyCookieManager_getIsUpdatePending(env, java_ref_);
  if (is_update_pending_in_java == is_update_pending_in_java_)
    return;
  is_update_pending_in_java_ = is_update_pending_in_java;
  UpdateCookie();
}

std::string ConsistencyCookieManagerAndroid::CalculateCookieValue() {
  if (is_update_pending_in_java_) {
    return kStateUpdating;
  }
  return ConsistencyCookieManagerBase::CalculateCookieValue();
}

}  // namespace signin
