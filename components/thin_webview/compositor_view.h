// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THIN_WEBVIEW_COMPOSITOR_VIEW_H_
#define COMPONENTS_THIN_WEBVIEW_COMPOSITOR_VIEW_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"

namespace cc::slim {
class Layer;
}  // namespace cc::slim

namespace thin_webview {
namespace android {

// Native interface for the CompositorView.java.
class CompositorView {
 public:
  static CompositorView* FromJavaObject(
      const base::android::JavaRef<jobject>& jcompositor_view);

  // Called to set the root layer of the view.
  virtual void SetRootLayer(scoped_refptr<cc::slim::Layer> layer) = 0;
};

}  // namespace android
}  // namespace thin_webview

#endif  // COMPONENTS_THIN_WEBVIEW_COMPOSITOR_VIEW_H_
