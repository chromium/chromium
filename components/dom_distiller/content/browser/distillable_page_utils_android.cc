// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/dom_distiller/content/browser/android/jni_headers/DistillablePageUtils_jni.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace dom_distiller {
namespace android {

namespace {

class JniDistillabilityObserverWrapper
    : public DistillabilityObserver,
      public content::WebContentsUserData<JniDistillabilityObserverWrapper> {
 public:
  void SetCallback(JNIEnv* env, const JavaParamRef<jobject>& callback) {
    callback_ = ScopedJavaGlobalRef<jobject>(env, callback);
  }

  void OnResult(const DistillabilityResult& result) override {
    Java_DistillablePageUtils_callOnIsPageDistillableUpdate(
        base::android::AttachCurrentThread(), callback_, result.is_distillable,
        result.is_last, result.is_mobile_friendly);
  }

 private:
  explicit JniDistillabilityObserverWrapper(content::WebContents* contents) {}
  friend class content::WebContentsUserData<JniDistillabilityObserverWrapper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  ScopedJavaGlobalRef<jobject> callback_;

  DISALLOW_COPY_AND_ASSIGN(JniDistillabilityObserverWrapper);
};

}  // namespace

static void JNI_DistillablePageUtils_SetDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& webContents,
    const JavaParamRef<jobject>& callback) {
  content::WebContents* web_contents(
      content::WebContents::FromJavaWebContents(webContents));
  if (!web_contents) {
    return;
  }

  JniDistillabilityObserverWrapper::CreateForWebContents(web_contents);
  auto* observer =
      JniDistillabilityObserverWrapper::FromWebContents(web_contents);
  DCHECK(observer);
  observer->SetCallback(env, callback);

  AddObserver(web_contents, observer);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(JniDistillabilityObserverWrapper)

}  // namespace android
}  // namespace dom_distiller
