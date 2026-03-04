// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/thin_webview/internal/thin_webview.h"

#include "base/android/jni_android.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_state.h"
#include "cc/slim/layer.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "components/thin_webview/thin_webview_initializer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/thin_webview/internal/jni_headers/ThinWebViewImpl_jni.h"

using base::android::JavaRef;
using web_contents_delegate_android::WebContentsDelegateAndroid;

namespace thin_webview {
namespace android {

static int64_t JNI_ThinWebViewImpl_Init(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& jcompositor_view,
    const JavaRef<jobject>& jwindow_android) {
  CompositorView* compositor_view =
      CompositorViewImpl::FromJavaObject(jcompositor_view);
  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  ThinWebView* view =
      new ThinWebView(env, obj, compositor_view, window_android);
  return reinterpret_cast<intptr_t>(view);
}

ThinWebView::ThinWebView(JNIEnv* env,
                         const base::android::JavaRef<jobject>& obj,
                         CompositorView* compositor_view,
                         ui::WindowAndroid* window_android)
    : obj_(env, obj),
      compositor_view_(compositor_view),
      window_android_(window_android),
      web_contents_(nullptr) {}

ThinWebView::~ThinWebView() = default;

void ThinWebView::Destroy(JNIEnv* env) {
  delete this;
}

void ThinWebView::PrimaryPageChanged(content::Page& page) {
  // Disable browser controls when used for thin webview.
  web_contents_->UpdateBrowserControlsState(cc::BrowserControlsState::kHidden,
                                            cc::BrowserControlsState::kHidden,
                                            false, std::nullopt);
}

void ThinWebView::SetWebContents(
    JNIEnv* env,
    const JavaRef<jobject>& jweb_contents,
    const JavaRef<jobject>& jweb_contents_delegate) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  WebContentsDelegateAndroid* delegate =
      jweb_contents_delegate.is_null()
          ? nullptr
          : new WebContentsDelegateAndroid(env, jweb_contents_delegate);
  SetWebContents(web_contents, delegate);
}

void ThinWebView::SetWebContents(content::WebContents* web_contents,
                                 WebContentsDelegateAndroid* delegate) {
  DCHECK(web_contents);
  web_contents_ = web_contents;
  Observe(web_contents_);
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  if (view_android->parent() != window_android_) {
    window_android_->AddChild(view_android);
  }

  compositor_view_->SetRootLayer(web_contents_->GetNativeView()->GetLayer());
  ResizeWebContents(view_size_);
  web_contents_delegate_.reset(delegate);
  if (delegate)
    web_contents->SetDelegate(delegate);

  ThinWebViewInitializer::GetInstance()->AttachTabHelpers(web_contents);
}

void ThinWebView::SetContextMenuPopulatorFactory(
    JNIEnv* env,
    const JavaRef<jobject>& jpopulator_factory) {
  if (!web_contents_) {
    return;
  }
  ThinWebViewInitializer::GetInstance()->SetContextMenuPopulatorFactory(
      web_contents_, jpopulator_factory);
}

// TODO (crbug.com/489707823) : Investigate physical backing changes.
void ThinWebView::SetInsets(JNIEnv* env,
                            int32_t top,
                            int32_t left,
                            int32_t bottom,
                            int32_t right) {
  insets_ = gfx::Insets::TLBR(top, left, bottom, right);
  if (web_contents_) {
    ResizeWebContents(view_size_);
  }
}

void ThinWebView::SizeChanged(JNIEnv* env, int32_t width, int32_t height) {
  view_size_ = gfx::Size(width, height);

  // TODO(shaktisahu): If we want to use a different size for WebContents, e.g.
  // showing full screen contents instead inside this view, don't do the resize.
  if (web_contents_)
    ResizeWebContents(view_size_);
}

void ThinWebView::ResizeWebContents(const gfx::Size& size) {
  if (!web_contents_)
    return;

  int width = std::max(0, size.width() - insets_.width());
  int height = std::max(0, size.height() - insets_.height());
  gfx::Size viewport_size(width, height);

  web_contents_->GetNativeView()->OnPhysicalBackingSizeChanged(viewport_size);
  web_contents_->GetNativeView()->OnSizeChanged(viewport_size.width(),
                                                viewport_size.height());

  // Position the WebContents layer to account for top/left insets.
  // Content is drawn at (0,0) in its own layer space, so we move the layer.
  if (auto* layer = web_contents_->GetNativeView()->GetLayer()) {
    layer->SetPosition(gfx::PointF(insets_.left(), insets_.top()));
  }
}

}  // namespace android
}  // namespace thin_webview

DEFINE_JNI(ThinWebViewImpl)
