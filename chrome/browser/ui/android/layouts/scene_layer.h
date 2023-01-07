// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_LAYOUTS_SCENE_LAYER_H_
#define CHROME_BROWSER_UI_ANDROID_LAYOUTS_SCENE_LAYER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "cc/layers/layer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace android {

// A native-side, cc::Layer based representation of how a scene should be drawn.
// Basically, this is a wrapper around cc::Layer and bridge with its Java
// counterpart.
class SceneLayer {
 public:
  static SceneLayer* FromJavaObject(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);

  // Create SceneLayer and creates an empty cc::Layer.
  SceneLayer(JNIEnv* env, const base::android::JavaRef<jobject>& jobj);
  // Create SceneLayer with the already-instantiated |layer|.

  SceneLayer(JNIEnv* env,
             const base::android::JavaRef<jobject>& jobj,
             scoped_refptr<cc::Layer> layer);

  SceneLayer(const SceneLayer&) = delete;
  SceneLayer& operator=(const SceneLayer&) = delete;

  virtual ~SceneLayer();

  // Notifies that this scene layer is about to be detached from its parent.
  // TODO(changwan): check if we can remove this.
  virtual void OnDetach();

  // Remove this SceneLayer from its current parent.
  virtual void RemoveFromParent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

  // Java SceneLayer can use this method to destroy its native-side counterpart.
  virtual void Destroy(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jobj);

  // Returns cc::Layer object that this SceneLayer contains.
  scoped_refptr<cc::Layer> layer() { return layer_; }

  // Returns whether we should show background when we draw this SceneLayer.
  virtual bool ShouldShowBackground();

  // Returns the background color if it is needed.
  virtual SkColor GetBackgroundColor();

 protected:
  JavaObjectWeakGlobalRef weak_java_scene_layer_;
  scoped_refptr<cc::Layer> layer_;
};

}  // namespace android

#endif  // CHROME_BROWSER_UI_ANDROID_LAYOUTS_SCENE_LAYER_H_
