// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_ANDROID_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/navigation_interception/navigation_params.h"

namespace navigation_interception {

base::android::ScopedJavaLocalRef<jobject> CreateJavaNavigationParams(
    JNIEnv* env,
    const NavigationParams& params);

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_ANDROID_H_
