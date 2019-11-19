// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/navigation_params_android.h"

#include "base/android/jni_string.h"
#include "components/navigation_interception/jni_headers/NavigationParams_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace navigation_interception {

base::android::ScopedJavaLocalRef<jobject> CreateJavaNavigationParams(
    JNIEnv* env,
    const NavigationParams& params,
    bool has_user_gesture_carryover) {
  const GURL& url = params.base_url_for_data_url().is_empty()
                        ? params.url()
                        : params.base_url_for_data_url();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.possibly_invalid_spec());

  ScopedJavaLocalRef<jstring> jstring_referrer =
      ConvertUTF8ToJavaString(env, params.referrer().url.spec());

  return Java_NavigationParams_create(
      env, jstring_url, jstring_referrer, params.is_post(),
      params.has_user_gesture(), params.transition_type(), params.is_redirect(),
      params.is_external_protocol(), params.is_main_frame(),
      params.is_renderer_initiated(), has_user_gesture_carryover);
}

}  // namespace navigation_interception
