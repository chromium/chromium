// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/android/content_capture_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/no_destructor.h"
#include "components/content_capture/android/jni_headers/ContentCaptureController_jni.h"
#include "third_party/re2/src/re2/re2.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::JavaBooleanArrayToBoolVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content_capture {

// static
jlong JNI_ContentCaptureController_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& jcaller) {
  auto* controller = ContentCaptureController::Get();
  controller->SetJavaPeer(env, jcaller);
  return reinterpret_cast<jlong>(controller);
}

// static
ContentCaptureController* ContentCaptureController::Get() {
  static base::NoDestructor<ContentCaptureController> s;
  return s.get();
}

ContentCaptureController::ContentCaptureController() = default;
ContentCaptureController::~ContentCaptureController() = default;

void ContentCaptureController::SetAllowlist(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobjectArray>& jallowlist,
    const JavaParamRef<jbooleanArray>& jisRegEx) {
  DCHECK(jallowlist && jisRegEx || !(jallowlist || jisRegEx));
  has_allowlist_ = false;
  if (!jallowlist)
    return;
  std::vector<std::string> allowlist;
  std::vector<bool> is_regex;
  AppendJavaStringArrayToStringVector(env, jallowlist, &allowlist);
  JavaBooleanArrayToBoolVector(env, jisRegEx, &is_regex);
  if (allowlist.size() != is_regex.size())
    return;
  has_allowlist_ = true;
  size_t index = 0;
  for (auto w : allowlist) {
    if (is_regex[index++])
      allowlist_regex_.push_back(std::make_unique<re2::RE2>(w));
    else
      allowlist_.push_back(w);
  }
}

void ContentCaptureController::SetJavaPeer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  java_ref_ = JavaObjectWeakGlobalRef(env, jcaller);
}

bool ContentCaptureController::ShouldCapture(const GURL& url) {
  if (!has_allowlist_.has_value()) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null())
      return false;
    Java_ContentCaptureController_pullAllowlist(env, obj);
  }
  DCHECK(has_allowlist_.has_value());

  // Everything is allowed.
  if (!has_allowlist_.value() || !url.has_host())
    return true;

  std::string host = url.host();
  for (auto& w : allowlist_) {
    if (w == host)
      return true;
  }

  for (auto& w : allowlist_regex_) {
    if (re2::RE2::FullMatch(host, *w))
      return true;
  }
  return false;
}

bool ContentCaptureController::ShouldCapture(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobjectArray>& urls) {
  std::vector<base::string16> url_list;
  AppendJavaStringArrayToStringVector(env, urls, &url_list);
  for (auto url : url_list) {
    if (!ShouldCapture(GURL(url)))
      return false;
  }
  return true;
}

}  // namespace content_capture
