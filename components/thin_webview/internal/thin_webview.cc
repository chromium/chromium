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

namespace thin_webview::android {

static jni_zero::JniUniquePtr<ThinWebView> JNI_ThinWebViewImpl_Init(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& jcompositor_view,
    const JavaRef<jobject>& jwindow_android) {
  CompositorView* compositor_view =
      CompositorViewImpl::FromJavaObject(jcompositor_view);
  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  return jni_zero::MakeUnique<ThinWebView>(env, obj, compositor_view,
                                           window_android);
}

ThinWebView::ThinWebView(JNIEnv* env,
                         const base::android::JavaRef<jobject>& obj,
                         CompositorView* compositor_view,
                         ui::WindowAndroid* window_android)
    : obj_(env, obj),
      compositor_view_(compositor_view),
      window_android_(window_android),
      web_contents_(nullptr) {}

ThinWebView::~ThinWebView() {
  ResetColorProviderSourceIfStillHosted();
}

void ThinWebView::PrimaryPageChanged(content::Page& page) {
  // Disable browser controls when used for thin webview.
  if (web_contents_) {
    web_contents_->UpdateBrowserControlsState(cc::BrowserControlsState::kHidden,
                                              cc::BrowserControlsState::kHidden,
                                              false, std::nullopt);
  }
}

void ThinWebView::SetWebContents(JNIEnv* env,
                                 const JavaRef<jobject>& jweb_contents,
                                 const JavaRef<jobject>& jweb_contents_delegate,
                                 bool enable_permission_requests,
                                 bool support_theming,
                                 bool enable_browser_autofill) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  WebContentsDelegateAndroid* delegate =
      jweb_contents_delegate.is_null()
          ? nullptr
          : new WebContentsDelegateAndroid(env, jweb_contents_delegate);
  SetWebContents(web_contents, delegate, enable_permission_requests,
                 support_theming, enable_browser_autofill);
}

void ThinWebView::SetWebContents(content::WebContents* web_contents,
                                 WebContentsDelegateAndroid* delegate,
                                 bool enable_permission_requests,
                                 bool support_theming,
                                 bool enable_browser_autofill) {
  DCHECK(web_contents);
  ThinWebViewInitializer* initializer = ThinWebViewInitializer::GetInstance();
  bool is_same_web_contents = (web_contents_.get() == web_contents);
  if (!is_same_web_contents) {
    ResetColorProviderSourceIfStillHosted();
  }

  Observe(web_contents);
  web_contents_ = web_contents->GetWeakPtr();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  // Capture the host Activity's WindowAndroid (set via setDelegates() /
  // setTopLevelNativeWindow()) before reparenting `view_android` under
  // `window_android_` (which may be ThinWebView's own unthemed window).
  ui::WindowAndroid* host_window = web_contents_->GetTopLevelNativeWindow();
  bool already_in_this_window = (view_android->parent() == window_android_);
  if (!already_in_this_window) {
    window_android_->AddChild(view_android);
  }

  compositor_view_->SetRootLayer(web_contents_->GetNativeView()->GetLayer());
  if (!view_size_.IsEmpty()) {
    ResizeWebContents(view_size_);
  }

  if (support_theming) {
    initializer->SetUpTheming(web_contents);
  }

  // Skip if re-attaching the same WebContents while it is already parented
  // under `window_android_`, so we do not replace its host Activity's
  // ColorProviderSource with ThinWebView's internal window.
  if (!is_same_web_contents || !already_in_this_window) {
    initializer->UpdateColorProviderSource(web_contents, host_window);
  }

  if (support_theming) {
    web_contents->OnWebPreferencesChanged();
  }

  web_contents_delegate_.reset(delegate);
  if (delegate) {
    web_contents->SetDelegate(delegate);
  }

  initializer->AttachTabHelpers(web_contents, enable_permission_requests,
                                enable_browser_autofill);
}

void ThinWebView::ResetColorProviderSourceIfStillHosted() {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    return;
  }
  ui::WindowAndroid* current_window = web_contents_->GetTopLevelNativeWindow();
  if (!current_window || current_window == window_android_) {
    if (auto* initializer = ThinWebViewInitializer::GetInstance()) {
      initializer->UpdateColorProviderSource(web_contents_.get(),
                                             /*host_window=*/nullptr);
    }
  }
}

void ThinWebView::SetContextMenuPopulatorFactory(
    JNIEnv* env,
    const JavaRef<jobject>& jpopulator_factory) {
  if (!web_contents_) {
    return;
  }
  ThinWebViewInitializer::GetInstance()->SetContextMenuPopulatorFactory(
      web_contents_.get(), jpopulator_factory);
}

void ThinWebView::SizeChanged(JNIEnv* env, int32_t width, int32_t height) {
  view_size_ = gfx::Size(width, height);

  // TODO(shaktisahu): If we want to use a different size for WebContents, e.g.
  // showing full screen contents instead inside this view, don't do the resize.
  if (web_contents_) {
    ResizeWebContents(view_size_);
  }
}

void ThinWebView::ResizeWebContents(const gfx::Size& size) {
  if (!web_contents_) {
    return;
  }

  web_contents_->GetNativeView()->OnPhysicalBackingSizeChanged(size);
  web_contents_->GetNativeView()->OnSizeChanged(size.width(), size.height());
}

}  // namespace thin_webview::android

DEFINE_JNI(ThinWebViewImpl)
