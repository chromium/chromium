// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/csp_checker_android.h"

#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/CSPCheckerBridge_jni.h"

namespace payments {

CSPCheckerAndroid::CSPCheckerAndroid(
    const base::android::JavaParamRef<jobject>& jbridge)
    : jbridge_(jbridge) {}

CSPCheckerAndroid::~CSPCheckerAndroid() = default;

void CSPCheckerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void CSPCheckerAndroid::OnResult(JNIEnv* env,
                                 jint callback_id,
                                 jboolean result) {
  auto iter = result_callbacks_.find(callback_id);
  if (iter == result_callbacks_.end())
    return;

  base::OnceCallback<void(bool)> callback = std::move(iter->second);
  result_callbacks_.erase(iter);

  std::move(callback).Run(result);
}

// static
base::WeakPtr<CSPCheckerAndroid> CSPCheckerAndroid::GetWeakPtr(
    jlong native_csp_checker_android) {
  if (!native_csp_checker_android)
    return base::WeakPtr<CSPCheckerAndroid>();

  payments::CSPCheckerAndroid* csp_checker_android =
      reinterpret_cast<payments::CSPCheckerAndroid*>(
          native_csp_checker_android);
  if (!csp_checker_android)
    return base::WeakPtr<CSPCheckerAndroid>();

  return csp_checker_android->weak_ptr_factory_.GetWeakPtr();
}

void CSPCheckerAndroid::AllowConnectToSource(
    const GURL& url,
    const GURL& url_before_redirects,
    bool did_follow_redirect,
    base::OnceCallback<void(bool)> result_callback) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (!env)
    return;

  int callback_id = ++callback_counter_;
  result_callbacks_.insert(
      std::make_pair(callback_id, std::move(result_callback)));

  Java_CSPCheckerBridge_allowConnectToSource(
      env, jbridge_, url::GURLAndroid::FromNativeGURL(env, url),
      url::GURLAndroid::FromNativeGURL(env, url_before_redirects),
      did_follow_redirect, callback_id);
}

// A static free function declared in and invoked directly from Java.
static jlong JNI_CSPCheckerBridge_CreateNativeCSPChecker(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbridge) {
  return reinterpret_cast<intptr_t>(new CSPCheckerAndroid(jbridge));
}

}  // namespace payments
