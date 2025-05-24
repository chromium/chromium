// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distilled_page_prefs_android.h"
#include "components/dom_distiller/core/dom_distiller_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/dom_distiller/core/android/jni_headers/DomDistillerService_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace dom_distiller {
namespace android {

DomDistillerServiceAndroid::DomDistillerServiceAndroid(
    DomDistillerService* service)
    : service_(service) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_DomDistillerService_create(env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, local_java_ref.obj());
}

DomDistillerServiceAndroid::~DomDistillerServiceAndroid() = default;

jlong DomDistillerServiceAndroid::GetDistilledPagePrefsPtr(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(service_->GetDistilledPagePrefs());
}

}  // namespace android
}  // namespace dom_distiller
