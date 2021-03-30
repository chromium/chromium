// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/android/content_capture_receiver_manager_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/content_capture/android/jni_headers/ContentCaptureData_jni.h"
#include "components/content_capture/android/jni_headers/ContentCaptureFrame_jni.h"
#include "components/content_capture/android/jni_headers/ContentCaptureReceiverManager_jni.h"
#include "components/content_capture/common/content_capture_features.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaLongArray;

static ScopedJavaLocalRef<jobject>
JNI_ContentCaptureReceiverManager_CreateOrGet(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jwebContents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jwebContents);
  DCHECK(web_contents);
  auto* manager =
      content_capture::ContentCaptureReceiverManager::FromWebContents(
          web_contents);
  if (!manager) {
    manager = content_capture::ContentCaptureReceiverManagerAndroid::Create(
        env, web_contents);
  }
  return static_cast<content_capture::ContentCaptureReceiverManagerAndroid*>(
             manager)
      ->GetJavaObject();
}

namespace content_capture {

namespace {

ScopedJavaLocalRef<jobject> ToJavaObjectOfContentCaptureData(
    JNIEnv* env,
    const ContentCaptureData& data,
    const JavaRef<jobject>& parent,
    int offset_y) {
  ScopedJavaLocalRef<jstring> jvalue =
      ConvertUTF16ToJavaString(env, data.value);
  ScopedJavaLocalRef<jobject> jdata =
      Java_ContentCaptureData_createContentCaptureData(
          env, parent, data.id, jvalue, data.bounds.x(),
          data.bounds.y() + offset_y, data.bounds.width(),
          data.bounds.height());
  if (jdata.is_null())
    return jdata;
  for (const auto& child : data.children) {
    ToJavaObjectOfContentCaptureData(env, child, jdata, offset_y);
  }
  return jdata;
}

ScopedJavaLocalRef<jobject> ToJavaObjectOfContentCaptureFrame(
    JNIEnv* env,
    const ContentCaptureFrame& data,
    int offset_y) {
  ScopedJavaLocalRef<jstring> jurl = ConvertUTF16ToJavaString(env, data.url);
  ScopedJavaLocalRef<jstring> jtitle;
  if (!data.title.empty())
    jtitle = ConvertUTF16ToJavaString(env, data.title);

  ScopedJavaLocalRef<jobject> jdata =
      Java_ContentCaptureFrame_createContentCaptureFrame(
          env, data.id, jurl, data.bounds.x(), data.bounds.y() + offset_y,
          data.bounds.width(), data.bounds.height(), jtitle);
  if (jdata.is_null())
    return jdata;
  for (const auto& child : data.children) {
    ToJavaObjectOfContentCaptureData(env, child, jdata, offset_y);
  }
  return jdata;
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfContentCaptureFrame(
    JNIEnv* env,
    const ContentCaptureSession& session,
    int offset_y) {
  ScopedJavaLocalRef<jclass> object_clazz =
      base::android::GetClass(env, "java/lang/Object");
  jobjectArray joa =
      env->NewObjectArray(session.size(), object_clazz.obj(), nullptr);
  jni_generator::CheckException(env);

  for (size_t i = 0; i < session.size(); ++i) {
    ScopedJavaLocalRef<jobject> item =
        ToJavaObjectOfContentCaptureFrame(env, session[i], offset_y);
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

}  // namespace

ContentCaptureReceiverManagerAndroid::ContentCaptureReceiverManagerAndroid(
    JNIEnv* env,
    content::WebContents* web_contents)
    : ContentCaptureReceiverManager(web_contents),
      java_ref_(Java_ContentCaptureReceiverManager_Constructor(env)) {
  AddConsumer(*this);
}

ContentCaptureReceiverManagerAndroid::~ContentCaptureReceiverManagerAndroid() =
    default;

ContentCaptureReceiverManagerAndroid*
ContentCaptureReceiverManagerAndroid::Create(
    JNIEnv* env,
    content::WebContents* web_contents) {
  auto* manager = FromWebContents(web_contents);
  if (manager)
    return static_cast<ContentCaptureReceiverManagerAndroid*>(manager);
  return new ContentCaptureReceiverManagerAndroid(env, web_contents);
}

void ContentCaptureReceiverManagerAndroid::DidCaptureContent(
    const ContentCaptureSession& parent_session,
    const ContentCaptureFrame& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  const int offset_y = Java_ContentCaptureReceiverManager_getOffsetY(
      env, java_ref_, web_contents()->GetJavaWebContents());
  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureFrame(env, data, offset_y);
  if (jdata.is_null())
    return;
  Java_ContentCaptureReceiverManager_didCaptureContent(
      env, java_ref_,
      ToJavaArrayOfContentCaptureFrame(env, parent_session, offset_y), jdata);
}

void ContentCaptureReceiverManagerAndroid::DidUpdateContent(
    const ContentCaptureSession& parent_session,
    const ContentCaptureFrame& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  const int offset_y = Java_ContentCaptureReceiverManager_getOffsetY(
      env, java_ref_, web_contents()->GetJavaWebContents());
  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureFrame(env, data, offset_y);
  if (jdata.is_null())
    return;
  Java_ContentCaptureReceiverManager_didUpdateContent(
      env, java_ref_,
      ToJavaArrayOfContentCaptureFrame(env, parent_session, offset_y), jdata);
}

void ContentCaptureReceiverManagerAndroid::DidRemoveContent(
    const ContentCaptureSession& session,
    const std::vector<int64_t>& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());
  const int offset_y = Java_ContentCaptureReceiverManager_getOffsetY(
      env, java_ref_, web_contents()->GetJavaWebContents());
  Java_ContentCaptureReceiverManager_didRemoveContent(
      env, java_ref_, ToJavaArrayOfContentCaptureFrame(env, session, offset_y),
      ToJavaLongArray(env, data));
}

void ContentCaptureReceiverManagerAndroid::DidRemoveSession(
    const ContentCaptureSession& session) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());
  const int offset_y = Java_ContentCaptureReceiverManager_getOffsetY(
      env, java_ref_, web_contents()->GetJavaWebContents());
  Java_ContentCaptureReceiverManager_didRemoveSession(
      env, java_ref_, ToJavaArrayOfContentCaptureFrame(env, session, offset_y));
}

void ContentCaptureReceiverManagerAndroid::DidUpdateTitle(
    const ContentCaptureFrame& main_frame) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());
  const int offset_y = Java_ContentCaptureReceiverManager_getOffsetY(
      env, java_ref_, web_contents()->GetJavaWebContents());
  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureFrame(env, main_frame, offset_y);
  if (jdata.is_null())
    return;
  Java_ContentCaptureReceiverManager_didUpdateTitle(env, java_ref_, jdata);
}

bool ContentCaptureReceiverManagerAndroid::ShouldCapture(const GURL& url) {
  // Capture all urls for experiment, the url will be checked
  // before the content is sent to the consumers.
  if (features::ShouldTriggerContentCaptureForExperiment())
    return true;
  JNIEnv* env = AttachCurrentThread();
  return Java_ContentCaptureReceiverManager_shouldCapture(
      env, java_ref_, ConvertUTF8ToJavaString(env, url.spec()));
}

ScopedJavaLocalRef<jobject>
ContentCaptureReceiverManagerAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

}  // namespace content_capture
