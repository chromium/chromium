// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"

#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/embedder_support/android/delegate/color_picker_bridge.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_request_body_android.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/android/color_utils_android.h"
#include "ui/android/view_android.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/web_contents_delegate_jni_headers/WebContentsDelegateAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::ColorChooser;
using content::RenderWidgetHostView;
using content::WebContents;
using content::WebContentsDelegate;

namespace web_contents_delegate_android {

WebContentsDelegateAndroid::WebContentsDelegateAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj)
    : weak_java_delegate_(env, obj) {}

WebContentsDelegateAndroid::~WebContentsDelegateAndroid() = default;

ScopedJavaLocalRef<jobject> WebContentsDelegateAndroid::GetJavaDelegate(
    JNIEnv* env) const {
  return weak_java_delegate_.get(env);
}

// ----------------------------------------------------------------------------
// WebContentsDelegate methods
// ----------------------------------------------------------------------------

std::unique_ptr<content::ColorChooser>
WebContentsDelegateAndroid::OpenColorChooser(
    WebContents* source,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return std::make_unique<ColorPickerBridge>(source, color, suggestions);
}

// OpenURLFromTab() will be called when we're performing a browser-intiated
// navigation. The most common scenario for this is opening new tabs (see
// RenderViewImpl::decidePolicyForNavigation for more details).
WebContents* WebContentsDelegateAndroid::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  const GURL& url = params.url;
  WindowOpenDisposition disposition = params.disposition;

  if (!source || (disposition != WindowOpenDisposition::CURRENT_TAB &&
                  disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
                  disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
                  disposition != WindowOpenDisposition::OFF_THE_RECORD)) {
    NOTIMPLEMENTED();
    return NULL;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return WebContentsDelegate::OpenURLFromTab(
        source, params, std::move(navigation_handle_callback));
  }

  if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
      disposition == WindowOpenDisposition::OFF_THE_RECORD) {
    ScopedJavaLocalRef<jobject> java_gurl =
        url::GURLAndroid::FromNativeGURL(env, url);
    ScopedJavaLocalRef<jstring> extra_headers =
        ConvertUTF8ToJavaString(env, params.extra_headers);
    ScopedJavaLocalRef<jobject> post_data =
        content::ConvertResourceRequestBodyToJavaObject(env, params.post_data);
    Java_WebContentsDelegateAndroid_openNewTab(
        env, obj, java_gurl, extra_headers, post_data,
        static_cast<int>(disposition), params.is_renderer_initiated);
    return NULL;
  }

  auto navigation_handle = source->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));

  if (navigation_handle_callback && navigation_handle) {
    std::move(navigation_handle_callback).Run(*navigation_handle);
  }

  return source;
}

void WebContentsDelegateAndroid::NavigationStateChanged(
    WebContents* source,
    content::InvalidateTypes changed_flags) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_navigationStateChanged(env, obj,
                                                         changed_flags);
}

void WebContentsDelegateAndroid::VisibleSecurityStateChanged(
    WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_visibleSSLStateChanged(env, obj);
}

void WebContentsDelegateAndroid::ActivateContents(WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_activateContents(env, obj);
}

void WebContentsDelegateAndroid::LoadingStateChanged(
    WebContents* source,
    bool should_show_loading_ui) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  Java_WebContentsDelegateAndroid_loadingStateChanged(env, obj,
                                                      should_show_loading_ui);
}

void WebContentsDelegateAndroid::RendererUnresponsive(
    WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_rendererUnresponsive(env, obj);
}

void WebContentsDelegateAndroid::RendererResponsive(
    WebContents* source,
    content::RenderWidgetHost* render_widget_host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_rendererResponsive(env, obj);
}

bool WebContentsDelegateAndroid::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  ScopedJavaLocalRef<jobject> java_gurl =
      url::GURLAndroid::FromNativeGURL(env, target_url);
  return !Java_WebContentsDelegateAndroid_shouldCreateWebContents(env, obj,
                                                                  java_gurl);
}

void WebContentsDelegateAndroid::WebContentsCreated(
    WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    WebContents* new_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jobject> jsource_contents;
  if (source_contents)
    jsource_contents = source_contents->GetJavaWebContents();
  ScopedJavaLocalRef<jobject> jnew_contents;
  if (new_contents)
    jnew_contents = new_contents->GetJavaWebContents();

  ScopedJavaLocalRef<jobject> java_gurl =
      url::GURLAndroid::FromNativeGURL(env, target_url);
  Java_WebContentsDelegateAndroid_webContentsCreated(
      env, obj, jsource_contents, opener_render_process_id,
      opener_render_frame_id,
      base::android::ConvertUTF8ToJavaString(env, frame_name), java_gurl,
      jnew_contents);
}

void WebContentsDelegateAndroid::CloseContents(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_closeContents(env, obj);
}

void WebContentsDelegateAndroid::SetContentsBounds(WebContents* source,
                                                   const gfx::Rect& bounds) {
  // Do nothing.
}

bool WebContentsDelegateAndroid::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::DidAddMessageToConsole(
        source, log_level, message, line_no, source_id);
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(env, message));
  ScopedJavaLocalRef<jstring> jsource_id(
      ConvertUTF16ToJavaString(env, source_id));
  int jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG;
  switch (log_level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG;
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_LOG;
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_WARNING;
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_ERROR;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return Java_WebContentsDelegateAndroid_addMessageToConsole(
      env, GetJavaDelegate(env), jlevel, jmessage, line_no, jsource_id);
}

// This is either called from TabContents::DidNavigateMainFramePostCommit() with
// an empty GURL or responding to RenderViewHost::OnMsgUpateTargetURL(). In
// Chrome, the latter is not always called, especially not during history
// navigation. So we only handle the first case and pass the source TabContents'
// url to Java to update the UI.
void WebContentsDelegateAndroid::UpdateTargetURL(WebContents* source,
                                                 const GURL& url) {
  if (!url.is_empty())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_onUpdateUrl(
      env, obj, url::GURLAndroid::FromNativeGURL(env, source->GetVisibleURL()));
}

bool WebContentsDelegateAndroid::HandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  const JavaRef<jobject>& key_event = event.os_event;
  if (!key_event.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
    if (obj.is_null())
      return true;
    Java_WebContentsDelegateAndroid_handleKeyboardEvent(env, obj, key_event);
  }
  return true;
}

bool WebContentsDelegateAndroid::TakeFocus(WebContents* source, bool reverse) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::TakeFocus(source, reverse);
  return Java_WebContentsDelegateAndroid_takeFocus(env, obj, reverse);
}

void WebContentsDelegateAndroid::ShowRepostFormWarningDialog(
    WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_showRepostFormWarningDialog(env, obj);
}

bool WebContentsDelegateAndroid::ShouldBlockMediaRequest(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  ScopedJavaLocalRef<jobject> j_gurl =
      url::GURLAndroid::FromNativeGURL(env, url);
  return Java_WebContentsDelegateAndroid_shouldBlockMediaRequest(env, obj,
                                                                 j_gurl);
}

void WebContentsDelegateAndroid::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_enterFullscreenModeForTab(
      env, obj, options.prefers_navigation_bar, options.prefers_status_bar);
}

void WebContentsDelegateAndroid::FullscreenStateChangedForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_fullscreenStateChangedForTab(
      env, obj, options.prefers_navigation_bar, options.prefers_status_bar);
}

void WebContentsDelegateAndroid::ExitFullscreenModeForTab(
    WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_exitFullscreenModeForTab(env, obj);
}

bool WebContentsDelegateAndroid::IsFullscreenForTabOrPending(
    const WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_WebContentsDelegateAndroid_isFullscreenForTabOrPending(env, obj);
}

void WebContentsDelegateAndroid::OnDidBlockNavigation(
    content::WebContents* web_contents,
    const GURL& initiator_url,
    const GURL& blocked_url,
    blink::mojom::NavigationBlockedReason reason) {}

int WebContentsDelegateAndroid::GetTopControlsHeight() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return 0;
  return Java_WebContentsDelegateAndroid_getTopControlsHeight(env, obj);
}

int WebContentsDelegateAndroid::GetTopControlsMinHeight() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return 0;
  return Java_WebContentsDelegateAndroid_getTopControlsMinHeight(env, obj);
}

int WebContentsDelegateAndroid::GetBottomControlsHeight() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return 0;
  return Java_WebContentsDelegateAndroid_getBottomControlsHeight(env, obj);
}

int WebContentsDelegateAndroid::GetBottomControlsMinHeight() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return 0;
  return Java_WebContentsDelegateAndroid_getBottomControlsMinHeight(env, obj);
}

bool WebContentsDelegateAndroid::ShouldAnimateBrowserControlsHeightChanges() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_WebContentsDelegateAndroid_shouldAnimateBrowserControlsHeightChanges(
      env, obj);
}

bool WebContentsDelegateAndroid::DoBrowserControlsShrinkRendererSize(
    content::WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_WebContentsDelegateAndroid_controlsResizeView(env, obj);
}

int WebContentsDelegateAndroid::GetVirtualKeyboardHeight(
    content::WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_WebContentsDelegateAndroid_getVirtualKeyboardHeight(env, obj);
}

blink::mojom::DisplayMode WebContentsDelegateAndroid::GetDisplayMode(
    const content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return blink::mojom::DisplayMode::kUndefined;

  return static_cast<blink::mojom::DisplayMode>(
      Java_WebContentsDelegateAndroid_getDisplayModeChecked(env, obj));
}

void WebContentsDelegateAndroid::DidChangeCloseSignalInterceptStatus() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsDelegateAndroid_didChangeCloseSignalInterceptStatus(env, obj);
}

bool WebContentsDelegateAndroid::MaybeCopyContentAreaAsBitmap(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return false;
  }
  std::unique_ptr<base::OnceCallback<void(const SkBitmap&)>> wrapped_callback =
      std::make_unique<base::OnceCallback<void(const SkBitmap&)>>(
          std::move(callback));
  if (Java_WebContentsDelegateAndroid_maybeCopyContentAreaAsBitmap(
          env, obj, reinterpret_cast<jlong>(wrapped_callback.get()))) {
    // Ownership of callback has been transferred to java side and will be
    // transferred back in |MaybeCopyContentAreaAsBitmapOutcome|.
    wrapped_callback.release();
    return true;
  }
  return false;
}

SkBitmap WebContentsDelegateAndroid::MaybeCopyContentAreaAsBitmapSync() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return SkBitmap();
  }
  ScopedJavaLocalRef<jobject> bitmap =
      Java_WebContentsDelegateAndroid_maybeCopyContentAreaAsBitmapSync(env,
                                                                       obj);
  if (bitmap.is_null()) {
    return SkBitmap();
  }
  gfx::JavaBitmap java_bitmap_lock(bitmap);
  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(java_bitmap_lock);
  skbitmap.setImmutable();
  return skbitmap;
}

void WebContentsDelegateAndroid::DidBackForwardTransitionAnimationChange() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_didBackForwardTransitionAnimationChange(env,
                                                                          obj);
}

content::BackForwardTransitionAnimationManager::FallbackUXConfig
WebContentsDelegateAndroid::GetBackForwardTransitionFallbackUXConfig() {
  JNIEnv* env = AttachCurrentThread();
  // Java colors are already in 32bit ARBG, same as `SkColor`.
  jint favicon_background =
      Java_WebContentsDelegateAndroid_getBackForwardTransitionFallbackUXFaviconBackgroundColor(
          env, GetJavaDelegate(env));
  jint page_background =
      Java_WebContentsDelegateAndroid_getBackForwardTransitionFallbackUXPageBackgroundColor(
          env, GetJavaDelegate(env));
  return {
      .rounded_rectangle_color =
          SkColor4f::FromColor(static_cast<SkColor>(favicon_background)),
      .background_color =
          SkColor4f::FromColor(static_cast<SkColor>(page_background)),
  };
}

void WebContentsDelegateAndroid::ContentsZoomChange(bool zoom_in) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsDelegateAndroid_contentsZoomChange(env, GetJavaDelegate(env),
                                                     zoom_in);
}

void JNI_WebContentsDelegateAndroid_MaybeCopyContentAreaAsBitmapOutcome(
    JNIEnv* env,
    jlong callback_ptr,
    const base::android::JavaParamRef<jobject>& bitmap) {
  std::unique_ptr<base::OnceCallback<void(const SkBitmap&)>> callback(
      reinterpret_cast<base::OnceCallback<void(const SkBitmap&)>*>(
          callback_ptr));
  if (bitmap.is_null()) {
    // Failed because of Out of Memory Error.
    // Pass in an empty bitmap, rather than null in this case.
    std::move(*callback).Run(SkBitmap());
  } else {
    gfx::JavaBitmap java_bitmap_lock(bitmap);
    SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(java_bitmap_lock);
    skbitmap.setImmutable();
    CHECK(!skbitmap.drawsNothing());
    std::move(*callback).Run(skbitmap);
  }
}

}  // namespace web_contents_delegate_android
