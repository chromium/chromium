// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/toolbar_model_android.h"

#include "base/android/jni_string.h"
#include "components/omnibox/browser/toolbar_model_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "jni/ToolbarModel_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ToolbarModelAndroid::ToolbarModelAndroid(JNIEnv* env,
                                         const JavaRef<jobject>& obj)
    : toolbar_model_(
          std::make_unique<ToolbarModelImpl>(this,
                                             content::kMaxURLDisplayChars)),
      java_object_(obj) {}

ToolbarModelAndroid::~ToolbarModelAndroid() {
}

void ToolbarModelAndroid::Destroy(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetFormattedFullURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF16ToJavaString(
      env, toolbar_model_->GetFormattedFullURL());
}

ScopedJavaLocalRef<jstring> ToolbarModelAndroid::GetURLForDisplay(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return base::android::ConvertUTF16ToJavaString(
      env, toolbar_model_->GetURLForDisplay());
}

content::WebContents* ToolbarModelAndroid::GetActiveWebContents() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jweb_contents =
      Java_ToolbarModel_getActiveWebContents(env, java_object_);
  return content::WebContents::FromJavaWebContents(jweb_contents);
}

// static
jlong JNI_ToolbarModel_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new ToolbarModelAndroid(env, obj));
}
