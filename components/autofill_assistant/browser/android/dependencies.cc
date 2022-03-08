// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/dependencies.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "components/autofill_assistant/android/jni_headers_public/AssistantDependencies_jni.h"
#include "components/autofill_assistant/android/jni_headers_public/AssistantStaticDependencies_jni.h"

using ::base::android::AttachCurrentThread;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;

namespace autofill_assistant {

std::unique_ptr<Dependencies> Dependencies::CreateFromJavaStaticDependencies(
    const JavaRef<jobject>& jstatic_dependencies) {
  return base::WrapUnique(reinterpret_cast<Dependencies*>(
      Java_AssistantStaticDependencies_createNative(AttachCurrentThread(),
                                                    jstatic_dependencies)));
}

std::unique_ptr<Dependencies> Dependencies::CreateFromJavaDependencies(
    const JavaRef<jobject>& jdependencies) {
  const auto jstatic_dependencies =
      Java_AssistantDependencies_getStaticDependencies(AttachCurrentThread(),
                                                       jdependencies);
  return CreateFromJavaStaticDependencies(jstatic_dependencies);
}

Dependencies::Dependencies(JNIEnv* env,
                           const JavaParamRef<jobject>& jstatic_dependencies)
    : jstatic_dependencies_(jstatic_dependencies) {}

ScopedJavaGlobalRef<jobject> Dependencies::GetJavaStaticDependencies() const {
  return jstatic_dependencies_;
}

ScopedJavaGlobalRef<jobject> Dependencies::CreateInfoPageUtil() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createInfoPageUtil(
          AttachCurrentThread(), jstatic_dependencies_));
}

ScopedJavaGlobalRef<jobject> Dependencies::CreateAccessTokenUtil() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createAccessTokenUtil(
          AttachCurrentThread(), jstatic_dependencies_));
}

ScopedJavaGlobalRef<jobject> Dependencies::CreateImageFetcher() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createImageFetcher(
          AttachCurrentThread(), jstatic_dependencies_));
}

ScopedJavaGlobalRef<jobject> Dependencies::CreateIconBridge() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createIconBridge(AttachCurrentThread(),
                                                        jstatic_dependencies_));
}

bool Dependencies::IsAccessibilityEnabled() const {
  return Java_AssistantStaticDependencies_isAccessibilityEnabled(
      AttachCurrentThread(), jstatic_dependencies_);
}

Dependencies::~Dependencies() = default;

}  // namespace autofill_assistant
