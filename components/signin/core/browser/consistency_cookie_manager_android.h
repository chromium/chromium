// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_ANDROID_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/signin/core/browser/consistency_cookie_manager_base.h"

class SigninClient;

namespace signin {

class IdentityManager;

// ConsistencyCookieManagerAndroid subclasses ConsistencyCookieManagerBase to
// watch whether there are pending updates to the account list on the Java side.
class ConsistencyCookieManagerAndroid : public ConsistencyCookieManagerBase {
 public:
  ConsistencyCookieManagerAndroid(IdentityManager* identity_manager,
                                  SigninClient* signin_client,
                                  AccountReconcilor* reconcilor);

  ~ConsistencyCookieManagerAndroid() override;

  void OnIsUpdatePendingChanged(JNIEnv* env);

 protected:
  std::string CalculateCookieValue() override;

 private:
  bool is_update_pending_in_java_ = false;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  DISALLOW_COPY_AND_ASSIGN(ConsistencyCookieManagerAndroid);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_ANDROID_H_
