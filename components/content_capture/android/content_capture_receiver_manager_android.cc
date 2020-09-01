// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/android/content_capture_receiver_manager_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/content_capture/android/content_capture_controller.h"
#include "components/content_capture/android/jni_headers/ContentCaptureData_jni.h"
#include "components/content_capture/android/jni_headers/ContentCaptureReceiverManager_jni.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
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
    const JavaRef<jobject>& parent) {
  ScopedJavaLocalRef<jstring> jvalue =
      ConvertUTF16ToJavaString(env, data.value);
  ScopedJavaLocalRef<jobject> jdata =
      Java_ContentCaptureData_createContentCaptureData(
          env, parent, data.id, jvalue, data.bounds.x(), data.bounds.y(),
          data.bounds.width(), data.bounds.height());
  if (jdata.is_null())
    return jdata;
  for (auto child : data.children) {
    ToJavaObjectOfContentCaptureData(env, child, jdata);
  }
  return jdata;
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfContentCaptureData(
    JNIEnv* env,
    const ContentCaptureSession& session) {
  ScopedJavaLocalRef<jclass> object_clazz =
      base::android::GetClass(env, "java/lang/Object");
  jobjectArray joa =
      env->NewObjectArray(session.size(), object_clazz.obj(), NULL);
  jni_generator::CheckException(env);

  for (size_t i = 0; i < session.size(); ++i) {
    ScopedJavaLocalRef<jobject> item =
        ToJavaObjectOfContentCaptureData(env, session[i], JavaRef<jobject>());
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

}  // namespace

ContentCaptureReceiverManagerAndroid::ContentCaptureReceiverManagerAndroid(
    JNIEnv* env,
    content::WebContents* web_contents)
    : ContentCaptureReceiverManager(web_contents),
      java_ref_(Java_ContentCaptureReceiverManager_Constructor(env)) {}

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
    const ContentCaptureData& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureData(env, data, JavaRef<jobject>());
  if (jdata.is_null())
    return;
  Java_ContentCaptureReceiverManager_didCaptureContent(
      env, java_ref_, ToJavaArrayOfContentCaptureData(env, parent_session),
      jdata);
}

void ContentCaptureReceiverManagerAndroid::DidUpdateContent(
    const ContentCaptureSession& parent_session,
    const ContentCaptureData& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureData(env, data, JavaRef<jobject>());
  if (jdata.is_null())
    return;
  Java_ContentCaptureReceiverManager_didUpdateContent(
      env, java_ref_, ToJavaArrayOfContentCaptureData(env, parent_session),
      jdata);
}

void ContentCaptureReceiverManagerAndroid::DidRemoveContent(
    const ContentCaptureSession& session,
    const std::vector<int64_t>& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());
  Java_ContentCaptureReceiverManager_didRemoveContent(
      env, java_ref_, ToJavaArrayOfContentCaptureData(env, session),
      ToJavaLongArray(env, data));
}

void ContentCaptureReceiverManagerAndroid::DidRemoveSession(
    const ContentCaptureSession& session) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());
  Java_ContentCaptureReceiverManager_didRemoveSession(
      env, java_ref_, ToJavaArrayOfContentCaptureData(env, session));
}

bool ContentCaptureReceiverManagerAndroid::ShouldCapture(const GURL& url) {
  return ContentCaptureController::Get()->ShouldCapture(url);
}

ScopedJavaLocalRef<jobject>
ContentCaptureReceiverManagerAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

}  // namespace content_capture
