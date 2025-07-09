// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FIND_IN_PAGE_ANDROID_FIND_IN_PAGE_BRIDGE_H_
#define COMPONENTS_FIND_IN_PAGE_ANDROID_FIND_IN_PAGE_BRIDGE_H_

#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents.h"

namespace find_in_page {

class FindInPageBridge {
 public:
  FindInPageBridge(JNIEnv* env,
                   const base::android::JavaRef<jobject>& obj,
                   const base::android::JavaRef<jobject>& j_web_contents);

  FindInPageBridge(const FindInPageBridge&) = delete;
  FindInPageBridge& operator=(const FindInPageBridge&) = delete;

  void Destroy(JNIEnv*);

  void StartFinding(JNIEnv* env,
                    const base::android::JavaParamRef<jstring>& search_string,
                    jboolean forward_direction,
                    jboolean case_sensitive);

  void StopFinding(JNIEnv* env, jboolean clearSelection);

  base::android::ScopedJavaLocalRef<jstring> GetPreviousFindText(JNIEnv* env);

  void RequestFindMatchRects(JNIEnv* env, jint current_version);

  void ActivateNearestFindResult(JNIEnv* env, jfloat x, jfloat y);

  void ActivateFindInPageResultForAccessibility(JNIEnv* env);

 private:
  raw_ptr<content::WebContents> web_contents_;
  JavaObjectWeakGlobalRef weak_java_ref_;
};

}  // namespace find_in_page

#endif  // COMPONENTS_FIND_IN_PAGE_ANDROID_FIND_IN_PAGE_BRIDGE_H_
