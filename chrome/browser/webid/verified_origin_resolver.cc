// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/verified_origin_resolver.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/webid/jni_headers/VerifiedOriginResolver_jni.h"
#include "url/origin.h"

namespace content::webid {
VerifiedOriginResolver::VerifiedOriginResolver() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(Java_VerifiedOriginResolver_create(
      env, reinterpret_cast<intptr_t>(this)));
}

VerifiedOriginResolver::~VerifiedOriginResolver() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VerifiedOriginResolver_destroy(env, java_obj_);
}

void VerifiedOriginResolver::Resolve(const url::Origin& origin,
                                     ResolveCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  callback_ = std::move(callback);
  Java_VerifiedOriginResolver_resolve(env, java_obj_, origin);
}

// static
void VerifiedOriginResolver::AddVerificationOverrideForTesting(  // IN-TEST
    const std::string& package_name,
    const url::Origin& origin) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VerifiedOriginResolver_addVerificationOverrideForTesting(  // IN-TEST
      env, package_name, origin);
}

void VerifiedOriginResolver::OnOriginResolved(JNIEnv* env,
                                              const std::string& package_name,
                                              const std::string& service_name) {
  if (!callback_) {
    return;
  }
  if (package_name.empty() || service_name.empty()) {
    std::move(callback_).Run(base::unexpected(ResolveError::kNoServiceFound));
  } else {
    std::move(callback_).Run(std::make_pair(package_name, service_name));
  }
}

}  // namespace content::webid

DEFINE_JNI_FOR_VerifiedOriginResolver()  // NOLINT(readability/fn_size)
