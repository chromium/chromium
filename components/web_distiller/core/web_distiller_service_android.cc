// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/core/web_distiller_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/web_distiller/core/distilled_page_prefs.h"
#include "components/web_distiller/core/distilled_page_prefs_android.h"
#include "components/web_distiller/core/web_distiller_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/web_distiller/core/android/jni_headers/WebDistillerService_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace web_distiller {
namespace android {

WebDistillerServiceAndroid::WebDistillerServiceAndroid(
    WebDistillerService* service)
    : service_(service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_WebDistillerService_create(env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, local_java_ref);
}

WebDistillerServiceAndroid::~WebDistillerServiceAndroid() = default;

int64_t WebDistillerServiceAndroid::GetDistilledPagePrefsPtr(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(service_->GetDistilledPagePrefs());
}

}  // namespace android
}  // namespace web_distiller

DEFINE_JNI(WebDistillerService)
