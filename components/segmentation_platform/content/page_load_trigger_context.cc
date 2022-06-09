// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/content/page_load_trigger_context.h"

#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/segmentation_platform/content/jni_headers/PageLoadTriggerContext_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace segmentation_platform {

PageLoadTriggerContext::PageLoadTriggerContext(
    content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {}

PageLoadTriggerContext::~PageLoadTriggerContext() = default;

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
PageLoadTriggerContext::CreateJavaObject() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_web_contents;
  if (web_contents_ && !web_contents_->IsBeingDestroyed())
    j_web_contents = web_contents_->GetJavaWebContents();
  return Java_PageLoadTriggerContext_createPageLoadTriggerContext(
      env, j_web_contents);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace segmentation_platform
