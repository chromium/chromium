// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/find_in_page/android/find_in_page_bridge.h"

#include "base/android/jni_string.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/find_in_page/android/jni_headers/FindInPageBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace find_in_page {

FindInPageBridge::FindInPageBridge(JNIEnv* env,
                                   const JavaRef<jobject>& obj,
                                   const JavaRef<jobject>& j_web_contents)
    : weak_java_ref_(env, obj) {
  web_contents_ = content::WebContents::FromJavaWebContents(j_web_contents);
}

void FindInPageBridge::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  delete this;
}

void FindInPageBridge::StartFinding(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    const JavaParamRef<jstring>& search_string,
                                    jboolean forward_direction,
                                    jboolean case_sensitive) {
  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->StartFinding(
          base::android::ConvertJavaStringToUTF16(env, search_string),
          forward_direction, case_sensitive,
          true /* find_match */);
}

void FindInPageBridge::StopFinding(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jboolean clearSelection) {
  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->StopFinding(clearSelection ? find_in_page::SelectionAction::kClear
                                   : find_in_page::SelectionAction::kKeep);
}

ScopedJavaLocalRef<jstring> FindInPageBridge::GetPreviousFindText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF16ToJavaString(
      env, find_in_page::FindTabHelper::FromWebContents(web_contents_)
               ->previous_find_text());
}

void FindInPageBridge::RequestFindMatchRects(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jint current_version) {
  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->RequestFindMatchRects(current_version);
}

void FindInPageBridge::ActivateNearestFindResult(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jfloat x,
    jfloat y) {
  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->ActivateNearestFindResult(x, y);
}

void FindInPageBridge::ActivateFindInPageResultForAccessibility(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->ActivateFindInPageResultForAccessibility();
}

// static
static jlong JNI_FindInPageBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  FindInPageBridge* bridge = new FindInPageBridge(env, obj, j_web_contents);
  return reinterpret_cast<intptr_t>(bridge);
}

}  // namespace find_in_page
