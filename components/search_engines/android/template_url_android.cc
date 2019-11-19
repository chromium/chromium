// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/android/template_url_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/search_engines/template_url.h"

#include "components/search_engines/android/jni_headers/TemplateUrl_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

TemplateURL* ToTemplateURL(jlong j_template_url) {
  return reinterpret_cast<TemplateURL*>(j_template_url);
}

ScopedJavaLocalRef<jstring> JNI_TemplateUrl_GetShortName(
    JNIEnv* env,
    jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return base::android::ConvertUTF16ToJavaString(env,
                                                 template_url->short_name());
}

ScopedJavaLocalRef<jstring> JNI_TemplateUrl_GetKeyword(JNIEnv* env,
                                                       jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return base::android::ConvertUTF16ToJavaString(env, template_url->keyword());
}

jboolean JNI_TemplateUrl_IsPrepopulatedOrCreatedByPolicy(
    JNIEnv* env,
    jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return template_url->prepopulate_id() > 0 ||
         template_url->created_by_policy() ||
         template_url->created_from_play_api();
}

jlong JNI_TemplateUrl_GetLastVisitedTime(JNIEnv* env, jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return template_url->last_visited().ToJavaTime();
}

jint JNI_TemplateUrl_GetPrepopulatedId(JNIEnv* env, jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return template_url->prepopulate_id();
}

ScopedJavaLocalRef<jobject> CreateTemplateUrlAndroid(
    JNIEnv* env,
    const TemplateURL* template_url) {
  return Java_TemplateUrl_create(env, reinterpret_cast<intptr_t>(template_url));
}

ScopedJavaLocalRef<jstring> JNI_TemplateUrl_GetURL(JNIEnv* env,
                                                   jlong template_url_ptr) {
  TemplateURL* template_url = ToTemplateURL(template_url_ptr);
  return base::android::ConvertUTF8ToJavaString(env, template_url->url());
}
