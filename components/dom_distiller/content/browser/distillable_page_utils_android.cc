// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/dom_distiller/content/browser/android/jni_headers/DistillablePageUtils_jni.h"

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
  JniDistillabilityObserverWrapper(const JniDistillabilityObserverWrapper&) =
      delete;
  JniDistillabilityObserverWrapper& operator=(
      const JniDistillabilityObserverWrapper&) = delete;

  void SetCallback(JNIEnv* env, const JavaParamRef<jobject>& callback) {
    callback_ = ScopedJavaGlobalRef<jobject>(env, callback);
  }

  void OnResult(const DistillabilityResult& result) override {
    Java_DistillablePageUtils_callOnIsPageDistillableUpdate(
        base::android::AttachCurrentThread(), callback_, result.is_distillable,
        result.is_last, result.is_long_article, result.is_mobile_friendly);
  }

 private:
  explicit JniDistillabilityObserverWrapper(content::WebContents* contents)
      : content::WebContentsUserData<JniDistillabilityObserverWrapper>(
            *contents) {}
  friend class content::WebContentsUserData<JniDistillabilityObserverWrapper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  ScopedJavaGlobalRef<jobject> callback_;
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(JniDistillabilityObserverWrapper);

}  // namespace android
}  // namespace dom_distiller
