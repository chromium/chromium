// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_CSP_CHECKER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_CSP_CHECKER_ANDROID_H_

#include <jni.h>
#include <map>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/core/csp_checker.h"

namespace payments {

// Forwarding calls to a Java implementation.
class CSPCheckerAndroid : public CSPChecker {
 public:
  explicit CSPCheckerAndroid(
      const base::android::JavaParamRef<jobject>& jbridge);
  ~CSPCheckerAndroid() override;

  CSPCheckerAndroid(const CSPCheckerAndroid&) = delete;
  CSPCheckerAndroid& operator=(const CSPCheckerAndroid&) = delete;

  // Message from Java to destroy this object.
  void Destroy(JNIEnv* env);

  // Message from Java to return the result.
  void OnResult(JNIEnv* env, jint result_id, jboolean result);

  // Convert a Java-owned CSPCheckerAndroid* pointer into a weak pointer.
  static base::WeakPtr<CSPCheckerAndroid> GetWeakPtr(
      jlong native_csp_checker_android);

 private:
  // CSPChecker implementation.
  void AllowConnectToSource(
      const GURL& url,
      const GURL& url_before_redirects,
      bool did_follow_redirect,
      base::OnceCallback<void(bool)> result_callback) override;

  base::android::ScopedJavaGlobalRef<jobject> jbridge_;
  std::map<int, base::OnceCallback<void(bool)>> result_callbacks_;
  int callback_counter_ = 0;

  base::WeakPtrFactory<CSPCheckerAndroid> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_CSP_CHECKER_ANDROID_H_
