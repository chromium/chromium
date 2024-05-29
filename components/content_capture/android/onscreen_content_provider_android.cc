// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/android/onscreen_content_provider_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/content_capture/common/content_capture_features.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/content_capture/android/jni_headers/ContentCaptureData_jni.h"
#include "components/content_capture/android/jni_headers/ContentCaptureFrame_jni.h"
#include "components/content_capture/android/jni_headers/OnscreenContentProvider_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaLongArray;

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

  ScopedJavaLocalRef<jstring> jfavicon;
  if (!data.favicon.empty())
    jfavicon = ConvertUTF8ToJavaString(env, data.favicon);

  ScopedJavaLocalRef<jobject> jdata =
      Java_ContentCaptureFrame_createContentCaptureFrame(
          env, data.id, jurl, data.bounds.x(), data.bounds.y() + offset_y,
          data.bounds.width(), data.bounds.height(), jtitle, jfavicon);
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
  jobjectArray joa =
      env->NewObjectArray(session.size(), jni_zero::g_object_class, nullptr);
  base::android::CheckException(env);

  for (size_t i = 0; i < session.size(); ++i) {
    ScopedJavaLocalRef<jobject> item =
        ToJavaObjectOfContentCaptureFrame(env, session[i], offset_y);
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

}  // namespace

static jlong JNI_OnscreenContentProvider_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jwebContents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jwebContents);
  DCHECK(web_contents);
  auto* provider = new content_capture::OnscreenContentProviderAndroid(
      env, jcaller, web_contents);
  return reinterpret_cast<intptr_t>(provider);
}

OnscreenContentProviderAndroid::OnscreenContentProviderAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobject,
    content::WebContents* web_contents)
    : java_ref_(jobject) {
  AttachToWebContents(web_contents);
}

OnscreenContentProviderAndroid::~OnscreenContentProviderAndroid() = default;

void OnscreenContentProviderAndroid::DidCaptureContent(
    const ContentCaptureSession& parent_session,
    const ContentCaptureFrame& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  auto* web_contents = GetWebContents();
  DCHECK(web_contents);
  const int offset_y = Java_OnscreenContentProvider_getOffsetY(
      env, java_ref_, web_contents->GetJavaWebContents());
  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureFrame(env, data, offset_y);
  if (jdata.is_null())
    return;
  Java_OnscreenContentProvider_didCaptureContent(
      env, java_ref_,
      ToJavaArrayOfContentCaptureFrame(env, parent_session, offset_y), jdata);
}

void OnscreenContentProviderAndroid::DidUpdateContent(
    const ContentCaptureSession& parent_session,
    const ContentCaptureFrame& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  auto* web_contents = GetWebContents();
  DCHECK(web_contents);
  const int offset_y = Java_OnscreenContentProvider_getOffsetY(
      env, java_ref_, web_contents->GetJavaWebContents());
  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureFrame(env, data, offset_y);
  if (jdata.is_null())
    return;
  Java_OnscreenContentProvider_didUpdateContent(
      env, java_ref_,
      ToJavaArrayOfContentCaptureFrame(env, parent_session, offset_y), jdata);
}

void OnscreenContentProviderAndroid::DidRemoveContent(
    const ContentCaptureSession& session,
    const std::vector<int64_t>& data) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  auto* web_contents = GetWebContents();
  DCHECK(web_contents);
  const int offset_y = Java_OnscreenContentProvider_getOffsetY(
      env, java_ref_, web_contents->GetJavaWebContents());
  Java_OnscreenContentProvider_didRemoveContent(
      env, java_ref_, ToJavaArrayOfContentCaptureFrame(env, session, offset_y),
      ToJavaLongArray(env, data));
}

void OnscreenContentProviderAndroid::DidRemoveSession(
    const ContentCaptureSession& session) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  auto* web_contents = GetWebContents();
  DCHECK(web_contents);
  const int offset_y = Java_OnscreenContentProvider_getOffsetY(
      env, java_ref_, web_contents->GetJavaWebContents());
  Java_OnscreenContentProvider_didRemoveSession(
      env, java_ref_, ToJavaArrayOfContentCaptureFrame(env, session, offset_y));
}

void OnscreenContentProviderAndroid::DidUpdateTitle(
    const ContentCaptureFrame& main_frame) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  auto* web_contents = GetWebContents();
  DCHECK(web_contents);
  const int offset_y = Java_OnscreenContentProvider_getOffsetY(
      env, java_ref_, web_contents->GetJavaWebContents());
  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureFrame(env, main_frame, offset_y);
  if (jdata.is_null())
    return;
  Java_OnscreenContentProvider_didUpdateTitle(env, java_ref_, jdata);
}

void OnscreenContentProviderAndroid::DidUpdateFavicon(
    const ContentCaptureFrame& main_frame) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(java_ref_.obj());

  auto* web_contents = GetWebContents();
  DCHECK(web_contents);
  const int offset_y = Java_OnscreenContentProvider_getOffsetY(
      env, java_ref_, web_contents->GetJavaWebContents());
  ScopedJavaLocalRef<jobject> jdata =
      ToJavaObjectOfContentCaptureFrame(env, main_frame, offset_y);
  if (jdata.is_null())
    return;
  Java_OnscreenContentProvider_didUpdateFavicon(env, java_ref_, jdata);
}

bool OnscreenContentProviderAndroid::ShouldCapture(const GURL& url) {
  // Capture all urls for experiment, the url will be checked
  // before the content is sent to the consumers.
  if (features::ShouldTriggerContentCaptureForExperiment())
    return true;
  JNIEnv* env = AttachCurrentThread();
  return Java_OnscreenContentProvider_shouldCapture(
      env, java_ref_, ConvertUTF8ToJavaString(env, url.spec()));
}

ScopedJavaLocalRef<jobject> OnscreenContentProviderAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

void OnscreenContentProviderAndroid::OnWebContentsChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  if (auto* web_contents =
          content::WebContents::FromJavaWebContents(jweb_contents)) {
    AttachToWebContents(web_contents);
  }
}

void OnscreenContentProviderAndroid::AttachToWebContents(
    content::WebContents* web_contents) {
  DetachFromWebContents();
  OnscreenContentProvider* provider =
      OnscreenContentProvider::FromWebContents(web_contents);
  if (!provider)
    provider = OnscreenContentProvider::Create(web_contents);
  provider->AddConsumer(*this);
  onscreen_content_provider_ = provider->GetWeakPtr();
}

void OnscreenContentProviderAndroid::DetachFromWebContents() {
  if (auto* provider = onscreen_content_provider_.get())
    provider->RemoveConsumer(*this);
}

void OnscreenContentProviderAndroid::Destroy(JNIEnv* env) {
  DetachFromWebContents();
  delete this;
}

content::WebContents* OnscreenContentProviderAndroid::GetWebContents() {
  if (auto* provider = onscreen_content_provider_.get())
    return provider->web_contents();
  return nullptr;
}

}  // namespace content_capture
