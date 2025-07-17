// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXTERNAL_INTENTS_ANDROID_TEST_CHILD_FRAME_NAVIGATION_OBSERVER_H_
#define COMPONENTS_EXTERNAL_INTENTS_ANDROID_TEST_CHILD_FRAME_NAVIGATION_OBSERVER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

using base::android::ScopedJavaGlobalRef;
using content::NavigationHandle;
using content::WebContents;
using content::WebContentsObserver;
using content::WebContentsUserData;

namespace external_intents {

class TestChildFrameNavigationObserver
    : public WebContentsObserver,
      public WebContentsUserData<TestChildFrameNavigationObserver> {
 public:
  ~TestChildFrameNavigationObserver() override;

  static void CreateForWebContents(WebContents* web_contents,
                                   JNIEnv* env,
                                   jobject java_test_observer);

 private:
  friend WebContentsUserData<TestChildFrameNavigationObserver>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit TestChildFrameNavigationObserver(WebContents* web_contents,
                                            JNIEnv* env,
                                            jobject java_test_observer);
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void DidStartNavigation(NavigationHandle* navigation_handle) override;

  ScopedJavaGlobalRef<jobject> java_test_observer_;
};

}  // namespace external_intents

#endif  // COMPONENTS_EXTERNAL_INTENTS_ANDROID_TEST_CHILD_FRAME_NAVIGATION_OBSERVER_H_
