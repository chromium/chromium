// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/layouts/scene_layer.h"

#include "cc/slim/layer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/layouts/layouts_jni_headers/SceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace android {

// static
SceneLayer* SceneLayer::FromJavaObject(JNIEnv* env,
                                       const JavaRef<jobject>& jobj) {
  if (jobj.is_null())
    return nullptr;
  return reinterpret_cast<SceneLayer*>(Java_SceneLayer_getNativePtr(env, jobj));
}

SceneLayer::SceneLayer(JNIEnv* env, const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj, cc::slim::Layer::Create()) {}

SceneLayer::SceneLayer(JNIEnv* env,
                       const JavaRef<jobject>& jobj,
                       scoped_refptr<cc::slim::Layer> layer)
    : weak_java_scene_layer_(env, jobj), layer_(std::move(layer)) {
  Java_SceneLayer_setNativePtr(env, jobj, reinterpret_cast<intptr_t>(this));
}

SceneLayer::~SceneLayer() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_scene_layer_.get(env);
  if (jobj.is_null())
    return;

  Java_SceneLayer_setNativePtr(
      env, jobj, reinterpret_cast<intptr_t>(static_cast<SceneLayer*>(nullptr)));
}

void SceneLayer::RemoveFromParent(JNIEnv* env,
                                  const JavaParamRef<jobject>& jobj) {
  layer()->RemoveFromParent();
}

void SceneLayer::OnDetach() {
  // TODO(crbug.com/40149397): Determine if this needed with the exposure of
  //                RemoveFromParent to java.
  layer()->RemoveFromParent();
}

void SceneLayer::Destroy(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  delete this;
}

bool SceneLayer::ShouldShowBackground() {
  return false;
}

SkColor SceneLayer::GetBackgroundColor() {
  return SK_ColorWHITE;
}

static jlong JNI_SceneLayer_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  SceneLayer* tree_provider = new SceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(tree_provider);
}

}  // namespace android
