// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/dependencies_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "components/autofill_assistant/android/jni_headers_public_dependencies/AssistantDependencies_jni.h"
#include "components/autofill_assistant/android/jni_headers_public_dependencies/AssistantStaticDependencies_jni.h"

using ::base::android::AttachCurrentThread;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;

namespace autofill_assistant {

std::unique_ptr<DependenciesAndroid>
DependenciesAndroid::CreateFromJavaStaticDependencies(
    const JavaRef<jobject>& jstatic_dependencies) {
  return base::WrapUnique(reinterpret_cast<DependenciesAndroid*>(
      Java_AssistantStaticDependencies_createNative(AttachCurrentThread(),
                                                    jstatic_dependencies)));
}

std::unique_ptr<DependenciesAndroid>
DependenciesAndroid::CreateFromJavaDependencies(
    const JavaRef<jobject>& jdependencies) {
  const auto jstatic_dependencies =
      Java_AssistantDependencies_getStaticDependencies(AttachCurrentThread(),
                                                       jdependencies);
  return CreateFromJavaStaticDependencies(jstatic_dependencies);
}

DependenciesAndroid::DependenciesAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies)
    : jstatic_dependencies_(jstatic_dependencies) {}

ScopedJavaGlobalRef<jobject> DependenciesAndroid::GetJavaStaticDependencies()
    const {
  return jstatic_dependencies_;
}

ScopedJavaGlobalRef<jobject> DependenciesAndroid::CreateInfoPageUtil() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createInfoPageUtil(
          AttachCurrentThread(), jstatic_dependencies_));
}

ScopedJavaGlobalRef<jobject> DependenciesAndroid::CreateAccessTokenUtil()
    const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createAccessTokenUtil(
          AttachCurrentThread(), jstatic_dependencies_));
}

ScopedJavaGlobalRef<jobject> DependenciesAndroid::CreateImageFetcher() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createImageFetcher(
          AttachCurrentThread(), jstatic_dependencies_));
}

ScopedJavaGlobalRef<jobject> DependenciesAndroid::CreateIconBridge() const {
  return ScopedJavaGlobalRef<jobject>(
      Java_AssistantStaticDependencies_createIconBridge(AttachCurrentThread(),
                                                        jstatic_dependencies_));
}

bool DependenciesAndroid::IsAccessibilityEnabled() const {
  return Java_AssistantStaticDependencies_isAccessibilityEnabled(
      AttachCurrentThread(), jstatic_dependencies_);
}

DependenciesAndroid::~DependenciesAndroid() = default;

}  // namespace autofill_assistant
