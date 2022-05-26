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
    : web_contents(web_contents) {}

PageLoadTriggerContext::~PageLoadTriggerContext() = default;

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
PageLoadTriggerContext::CreateJavaObject() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PageLoadTriggerContext_createPageLoadTriggerContext(
      env, web_contents->GetJavaWebContents());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace segmentation_platform
