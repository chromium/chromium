// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THIN_WEBVIEW_INTERNAL_THIN_WEBVIEW_H_
#define COMPONENTS_THIN_WEBVIEW_INTERNAL_THIN_WEBVIEW_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/thin_webview/internal/compositor_view_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/android/window_android.h"

namespace web_contents_delegate_android {
class WebContentsDelegateAndroid;
}  // namespace web_contents_delegate_android

namespace thin_webview {
namespace android {

// Native counterpart of ThinWebViewImpl.java.
class ThinWebView : public content::WebContentsObserver {
 public:
  ThinWebView(JNIEnv* env,
              jobject obj,
              CompositorView* compositor_view,
              ui::WindowAndroid* window_android);

  ThinWebView(const ThinWebView&) = delete;
  ThinWebView& operator=(const ThinWebView&) = delete;

  ~ThinWebView() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& object);

  void SetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      const base::android::JavaParamRef<jobject>& jweb_contents_delegate);

  void SizeChanged(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& object,
                   jint width,
                   jint height);

 private:
  // content::WebContentsObserver overrides:
  void PrimaryPageChanged(content::Page& page) override;

  void SetWebContents(
      content::WebContents* web_contents,
      web_contents_delegate_android::WebContentsDelegateAndroid* delegate);
  void ResizeWebContents(const gfx::Size& size);

  base::android::ScopedJavaGlobalRef<jobject> obj_;
  raw_ptr<CompositorView, DanglingUntriaged> compositor_view_;
  raw_ptr<ui::WindowAndroid> window_android_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<web_contents_delegate_android::WebContentsDelegateAndroid>
      web_contents_delegate_;
  gfx::Size view_size_;
};

}  // namespace android
}  // namespace thin_webview

#endif  // COMPONENTS_THIN_WEBVIEW_INTERNAL_THIN_WEBVIEW_H_
