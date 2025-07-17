// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/external_intents/android/test_child_frame_navigation_observer.h"

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/external_intents/android/test_support_java_jni_headers/TestChildFrameNavigationObserver_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using content::WebContents;
using content::WebContentsObserver;

namespace external_intents {

TestChildFrameNavigationObserver::TestChildFrameNavigationObserver(
    WebContents* web_contents,
    JNIEnv* env,
    jobject java_test_observer)
    : WebContentsObserver(web_contents),
      WebContentsUserData<TestChildFrameNavigationObserver>(*web_contents) {
  DCHECK(java_test_observer);
  java_test_observer_.Reset(env, java_test_observer);
}

TestChildFrameNavigationObserver::~TestChildFrameNavigationObserver() = default;

// static
void TestChildFrameNavigationObserver::CreateForWebContents(
    WebContents* web_contents,
    JNIEnv* env,
    jobject java_test_observer) {
  WebContentsUserData<TestChildFrameNavigationObserver>::CreateForWebContents(
      web_contents, env, java_test_observer);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TestChildFrameNavigationObserver);

void JNI_TestChildFrameNavigationObserver_CreateAndAttachToNativeWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_test_observer,
    const JavaParamRef<jobject>& java_web_contents) {
  WebContents* web_contents =
      WebContents::FromJavaWebContents(java_web_contents);
  CHECK(web_contents);

  TestChildFrameNavigationObserver::CreateForWebContents(web_contents, env,
                                                         java_test_observer);
}

void TestChildFrameNavigationObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  external_intents::Java_TestChildFrameNavigationObserver_didFinishNavigation(
      AttachCurrentThread(), java_test_observer_,
      navigation_handle->GetJavaNavigationHandle());
}

void TestChildFrameNavigationObserver::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  external_intents::Java_TestChildFrameNavigationObserver_didStartNavigation(
      AttachCurrentThread(), java_test_observer_,
      navigation_handle->GetJavaNavigationHandle());
}

}  // namespace external_intents
