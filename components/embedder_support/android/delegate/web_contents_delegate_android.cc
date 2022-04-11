// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"

#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/embedder_support/android/delegate/color_chooser_android.h"
#include "components/embedder_support/android/web_contents_delegate_jni_headers/WebContentsDelegateAndroid_jni.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_request_body_android.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/android/view_android.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

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

WebContentsDelegateAndroid::WebContentsDelegateAndroid(JNIEnv* env, jobject obj)
    : weak_java_delegate_(env, obj) {}

WebContentsDelegateAndroid::~WebContentsDelegateAndroid() {}

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
  return std::make_unique<ColorChooserAndroid>(source, color, suggestions);
}

// OpenURLFromTab() will be called when we're performing a browser-intiated
// navigation. The most common scenario for this is opening new tabs (see
// RenderViewImpl::decidePolicyForNavigation for more details).
WebContents* WebContentsDelegateAndroid::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
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
  if (obj.is_null())
    return WebContentsDelegate::OpenURLFromTab(source, params);

  if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
      disposition == WindowOpenDisposition::OFF_THE_RECORD) {
    JNIEnv* env = AttachCurrentThread();
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

  source->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));

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
      NOTREACHED();
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
    const content::NativeWebKeyboardEvent& event) {
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

blink::mojom::DisplayMode WebContentsDelegateAndroid::GetDisplayMode(
    const content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return blink::mojom::DisplayMode::kUndefined;

  return static_cast<blink::mojom::DisplayMode>(
      Java_WebContentsDelegateAndroid_getDisplayModeChecked(env, obj));
}

}  // namespace web_contents_delegate_android
