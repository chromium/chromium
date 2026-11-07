// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillable_page_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/dom_distiller/content/browser/android/jni_headers/DistillablePageUtils_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace dom_distiller::android {

namespace {

class JniDistillabilityObserverWrapper
    : public DistillabilityObserver,
      public content::WebContentsUserData<JniDistillabilityObserverWrapper> {
 public:
  ~JniDistillabilityObserverWrapper() override = default;

  JniDistillabilityObserverWrapper(const JniDistillabilityObserverWrapper&) =
      delete;
  JniDistillabilityObserverWrapper& operator=(
      const JniDistillabilityObserverWrapper&) = delete;

  void SetDelegate(JNIEnv* env, const JavaRef<jobject>& delegate) {
    delegate_ = delegate ? JavaObjectWeakGlobalRef(env, delegate)
                         : JavaObjectWeakGlobalRef();
  }

  void OnResult(const DistillabilityResult& result) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jobject> delegate = delegate_.get(env);
    if (!delegate) {
      return;
    }
    Java_DistillablePageUtils_callOnIsPageDistillableUpdate(
        env, delegate, result.url, result.is_distillable, result.is_last,
        result.is_long_article, result.is_mobile_friendly);
  }

 private:
  explicit JniDistillabilityObserverWrapper(content::WebContents* contents)
      : content::WebContentsUserData<JniDistillabilityObserverWrapper>(
            *contents) {}
  friend class content::WebContentsUserData<JniDistillabilityObserverWrapper>;

  JavaObjectWeakGlobalRef delegate_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace

static void JNI_DistillablePageUtils_SetDelegate(
    JNIEnv* env,
    content::WebContents* web_contents,
    const JavaRef<jobject>& callback) {
  if (!web_contents) {
    return;
  }

  // If for some reason the WebContents is reused, this will also reuse the same
  // instance of the observer, but with a possibly different callback.
  JniDistillabilityObserverWrapper::CreateForWebContents(web_contents);
  auto* observer =
      JniDistillabilityObserverWrapper::FromWebContents(web_contents);
  observer->SetDelegate(env, callback);

  AddObserver(web_contents, observer);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(JniDistillabilityObserverWrapper);

}  // namespace dom_distiller::android

DEFINE_JNI(DistillablePageUtils)
