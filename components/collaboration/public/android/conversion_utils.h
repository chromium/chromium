// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_ANDROID_CONVERSION_UTILS_H_
#define COMPONENTS_COLLABORATION_PUBLIC_ANDROID_CONVERSION_UTILS_H_

#include "base/android/jni_android.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/data_sharing/public/group_data.h"

namespace collaboration::conversion {

// Converts a CollaborationControllerDelegate::ResultCallback to a Java readable
// long.
jlong GetJavaResultCallbackPtr(
    CollaborationControllerDelegate::ResultCallback result);

// Converts a Java long obtained from GetJavaResultCallbackPtr() back into a
// ResultCallback.
std::unique_ptr<CollaborationControllerDelegate::ResultCallback>
GetNativeResultCallbackFromJava(jlong callback);

// Converts a exit base::OnceClosure to a Java readable long.
jlong GetJavaExitCallbackPtr(base::OnceClosure callback);

// Converts a Java long obtained from GetJavaExitCallbackPtr() back into a
// native callback.
std::unique_ptr<base::OnceClosure> GetNativeExitCallbackFromJava(
    jlong callback);

// Converts a result with group token callback to a Java readable long.
jlong GetJavaResultWithGroupTokenCallbackPtr(
    CollaborationControllerDelegate::ResultWithGroupTokenCallback result);

// Converts a Java long obtained from GetJavaResultWithGroupTokenCallbackPtr()
// back into a result with group token callback.
std::unique_ptr<CollaborationControllerDelegate::ResultWithGroupTokenCallback>
GetNativeResultWithGroupTokenCallbackFromJava(jlong callback);

// Converts a unique CollaborationControllerDelegate to a Java readable long.
jlong GetJavaDelegateUniquePtr(
    std::unique_ptr<CollaborationControllerDelegate> delegate);

// Converts a Java long obtained from GetJavaDelegateUniquePtr() back into a
// unique CollaborationControllerDelegate.
std::unique_ptr<CollaborationControllerDelegate> GetDelegateUniquePtrFromJava(
    jlong java_ptr);

}  // namespace collaboration::conversion

#endif  // COMPONENTS_COLLABORATION_PUBLIC_ANDROID_CONVERSION_UTILS_H_
