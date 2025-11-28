// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"

#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_callback.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "components/embedder_support/android/delegate/color_picker_bridge.h"
#include "components/embedder_support/android/delegate/screenshot_result.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_request_body_android.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/android/color_utils_android.h"
#include "ui/android/view_android.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/web_contents_delegate_jni_headers/WebContentsDelegateAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedHardwareBufferHandle;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::ColorChooser;
using content::RenderWidgetHostView;
using content::WebContents;
using content::WebContentsDelegate;

namespace {

// The amount of time to disallow repeated pointer lock calls after the user
// successfully escapes from one lock request.
constexpr base::TimeDelta kEffectiveUserEscapeDuration =
    base::Milliseconds(1250);

}  // namespace

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
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_navigationStateChanged(env, obj,
                                                         changed_flags);
}

void WebContentsDelegateAndroid::VisibleSecurityStateChanged(
    WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_visibleSSLStateChanged(env, obj);
}

void WebContentsDelegateAndroid::ActivateContents(WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_activateContents(env, obj);
}

void WebContentsDelegateAndroid::LoadingStateChanged(
    WebContents* source,
    bool should_show_loading_ui) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_loadingStateChanged(env, obj,
                                                      should_show_loading_ui);
}

void WebContentsDelegateAndroid::RendererUnresponsive(
    WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_rendererUnresponsive(env, obj);
}

void WebContentsDelegateAndroid::RendererResponsive(
    WebContents* source,
    content::RenderWidgetHost* render_widget_host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_rendererResponsive(env, obj);
}

bool WebContentsDelegateAndroid::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return false;
  }
  ScopedJavaLocalRef<jobject> java_gurl =
      url::GURLAndroid::FromNativeGURL(env, target_url);
  return !Java_WebContentsDelegateAndroid_shouldCreateWebContents(env, obj,
                                                                  java_gurl);
}

void WebContentsDelegateAndroid::CloseContents(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_closeContents(env, obj);
}

bool WebContentsDelegateAndroid::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return WebContentsDelegate::DidAddMessageToConsole(
        source, log_level, message, line_no, source_id);
  }
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
      env, obj, jlevel, jmessage, line_no, jsource_id);
}

// Called when the target URL under the cursor changes. For example, when
// the user hovers over a link. Passes the URL to the Java side.
void WebContentsDelegateAndroid::UpdateTargetURL(WebContents* source,
                                                 const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_onUpdateTargetUrl(
      env, obj, url::GURLAndroid::FromNativeGURL(env, url));
}

content::KeyboardEventProcessingResult
WebContentsDelegateAndroid::PreHandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.native_key_code == AKEYCODE_ESCAPE) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);

    if (!obj.is_null() &&
        Java_WebContentsDelegateAndroid_preHandleKeyboardEvent(
            env, obj, reinterpret_cast<intptr_t>(&event))) {
      return content::KeyboardEventProcessingResult::HANDLED;
    }

    // ExclusiveAccessManager handles the pointer lock escape.
    if (!base::FeatureList::IsEnabled(
            features::kEnableExclusiveAccessManager)) {
      auto* rwhva = source->GetTopLevelRenderWidgetHostView();
      if (rwhva && rwhva->IsPointerLocked()) {
        rwhva->UnlockPointer();
        pointer_lock_last_user_escape_time_ = base::TimeTicks::Now();
        return content::KeyboardEventProcessingResult::HANDLED;
      }
    }
  }

  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool WebContentsDelegateAndroid::HandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  const JavaRef<jobject>& key_event = event.os_event;
  if (!key_event.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
    if (obj.is_null()) {
      return true;
    }
    Java_WebContentsDelegateAndroid_handleKeyboardEvent(env, obj, key_event);
  }
  return true;
}

bool WebContentsDelegateAndroid::TakeFocus(WebContents* source, bool reverse) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return WebContentsDelegate::TakeFocus(source, reverse);
  }
  return Java_WebContentsDelegateAndroid_takeFocus(env, obj, reverse);
}

void WebContentsDelegateAndroid::ShowRepostFormWarningDialog(
    WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_showRepostFormWarningDialog(env, obj);
}

bool WebContentsDelegateAndroid::ShouldBlockMediaRequest(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return false;
  }
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
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_enterFullscreenModeForTab(
      env, obj, requesting_frame->GetJavaRenderFrameHost(),
      options.prefers_navigation_bar, options.prefers_status_bar,
      options.display_id);
}

void WebContentsDelegateAndroid::FullscreenStateChangedForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_fullscreenStateChangedForTab(
      env, obj, requesting_frame->GetJavaRenderFrameHost(),
      options.prefers_navigation_bar, options.prefers_status_bar,
      options.display_id);
}

void WebContentsDelegateAndroid::ExitFullscreenModeForTab(
    WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_exitFullscreenModeForTab(env, obj);
}

void WebContentsDelegateAndroid::RequestPointerLock(
    WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  if (!base::FeatureList::IsEnabled(blink::features::kPointerLockOnAndroid)) {
    // WebContentsDelegate call would reject the lock request with a
    // kUnknownError
    return WebContentsDelegate::RequestPointerLock(web_contents, user_gesture,
                                                   last_unlocked_by_target);
  }

  // TODO(https://crbug.com/415732870): reuse the ExclusiveAccessManager
  // This part is taken from PointerLockController, See
  // `PointerLockController::RequestToLockPointer()` for more info.
  if (!last_unlocked_by_target && !web_contents->IsFullscreen()) {
    if (!user_gesture) {
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kRequiresUserGesture);
      return;
    }
    if (base::TimeTicks::Now() <
        pointer_lock_last_user_escape_time_ + kEffectiveUserEscapeDuration) {
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kUserRejected);
      return;
    }
  }

  web_contents->GotResponseToPointerLockRequest(
      blink::mojom::PointerLockResult::kSuccess);
}

void WebContentsDelegateAndroid::RequestKeyboardLock(WebContents* web_contents,
                                                     bool esc_key_locked) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_requestKeyboardLock(env, obj, esc_key_locked);
}

void WebContentsDelegateAndroid::CancelKeyboardLockRequest(
    WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_cancelKeyboardLockRequest(env, obj);
}

bool WebContentsDelegateAndroid::IsFullscreenForTabOrPending(
    const WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return false;
  }
  return Java_WebContentsDelegateAndroid_isFullscreenForTabOrPending(env, obj);
}

content::FullscreenState WebContentsDelegateAndroid::GetFullscreenState(
    const WebContents* web_contents) const {
  content::FullscreenState state;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return state;
  }
  state = WebContentsDelegate::GetFullscreenState(web_contents);
  state.target_display_id =
      Java_WebContentsDelegateAndroid_getFullscreenTargetDisplay(env, obj);
  return state;
}

void WebContentsDelegateAndroid::OnDidBlockNavigation(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    blink::mojom::NavigationBlockedReason reason) {}

int WebContentsDelegateAndroid::GetTopControlsHeight() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return 0;
  }
  return Java_WebContentsDelegateAndroid_getTopControlsHeight(env, obj);
}

int WebContentsDelegateAndroid::GetTopControlsMinHeight() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return 0;
  }
  return Java_WebContentsDelegateAndroid_getTopControlsMinHeight(env, obj);
}

int WebContentsDelegateAndroid::GetBottomControlsHeight() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return 0;
  }
  return Java_WebContentsDelegateAndroid_getBottomControlsHeight(env, obj);
}

int WebContentsDelegateAndroid::GetBottomControlsMinHeight() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return 0;
  }
  return Java_WebContentsDelegateAndroid_getBottomControlsMinHeight(env, obj);
}

bool WebContentsDelegateAndroid::ShouldAnimateBrowserControlsHeightChanges() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return false;
  }
  return Java_WebContentsDelegateAndroid_shouldAnimateBrowserControlsHeightChanges(
      env, obj);
}

bool WebContentsDelegateAndroid::DoBrowserControlsShrinkRendererSize(
    content::WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return false;
  }
  return Java_WebContentsDelegateAndroid_controlsResizeView(env, obj);
}

int WebContentsDelegateAndroid::GetVirtualKeyboardHeight(
    content::WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return false;
  }
  return Java_WebContentsDelegateAndroid_getVirtualKeyboardHeight(env, obj);
}

blink::mojom::DisplayMode WebContentsDelegateAndroid::GetDisplayMode(
    const content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return blink::mojom::DisplayMode::kUndefined;
  }

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
  TRACE_EVENT("content",
              "WebContentsDelegateAndroid::MaybeCopyContentAreaAsBitmap");
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return false;
  }
  base::TimeTicks start_time = base::TimeTicks::Now();
  // Convert the C++ callback to a JNI callback using ToJniCallback.
  auto wrapped_callback = base::BindOnce(
      [](base::OnceCallback<void(const SkBitmap&)> callback,
         const ScreenshotResult& result) {
        TRACE_EVENT("content",
                    "WebContentsDelegateAndroid::MaybeCopyContentAreaAsBitmap::"
                    "Callback");
        if (!result) {
          // Failed because of Out of Memory Error.
          // Pass in an empty bitmap, rather than null in this case.
          std::move(callback).Run(SkBitmap());
        } else {
          SkBitmap skbitmap = result.GetBitmap();
          skbitmap.setImmutable();
          CHECK(!skbitmap.drawsNothing());
          std::move(callback).Run(skbitmap);
        }
      },
      std::move(callback));
  if (Java_WebContentsDelegateAndroid_maybeCopyContentAreaAsBitmap(
          env, obj,
          base::android::ToJniCallback(env, std::move(wrapped_callback)))) {
    base::UmaHistogramTimes("Android.MaybeCopyContentAreaAsBitmap.Time",
                            base::TimeTicks::Now() - start_time);
    return true;
  }
  return false;
}

bool WebContentsDelegateAndroid::MaybeCopyContentAreaAsHardwareBuffer(
    content::HardwareBufferResultCallback output_callback) {
  TRACE_EVENT(
      "content",
      "WebContentsDelegateAndroid::MaybeCopyContentAreaAsHardwareBuffer");
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.is_null()) {
    return false;
  }
  // Wrap the result C++ callback as a JNI callback and convert the types.
  auto wrapped_output_callback = base::BindOnce(
      [](content::HardwareBufferResultCallback output_callback,
         const ScreenshotResult& result) {
        if (!result) {
          std::move(output_callback)
              .Run(base::android::ScopedHardwareBufferHandle(),
                   base::ScopedClosureRunner());
          return;
        }
        std::move(output_callback)
            .Run(result.GetHardwareBuffer(), result.GetReleaseCallback());
      },
      std::move(output_callback));
  if (Java_WebContentsDelegateAndroid_maybeCopyContentAreaAsHardwareBuffer(
          env, java_delegate,
          base::android::ToJniCallback(env,
                                       std::move(wrapped_output_callback)))) {
    return true;
  }
  return false;
}

SkBitmap WebContentsDelegateAndroid::MaybeCopyContentAreaAsBitmapSync() {
  TRACE_EVENT("content",
              "WebContentsDelegateAndroid::MaybeCopyContentAreaAsBitmapSync");
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

SkBitmap WebContentsDelegateAndroid::
    GetBackForwardTransitionFallbackUXInternalPageIcon() {
  TRACE_EVENT("content",
              "WebContentsDelegateAndroid::"
              "GetBackForwardTransitionFallbackUXInternalPageIcon");
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return SkBitmap();
  }
  // Call Java's #getBackForwardTransitionFallbackUXInternalPageIcon via JNI.
  ScopedJavaLocalRef<jobject> bitmap =
      Java_WebContentsDelegateAndroid_getBackForwardTransitionFallbackUXInternalPageIcon(
          env, obj);
  if (bitmap.is_null()) {
    return SkBitmap();
  }

  // Covert bitmap to SkBitmap.
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
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return {};
  }
  // Java colors are already in 32bit ARBG, same as `SkColor`.
  jint favicon_background =
      Java_WebContentsDelegateAndroid_getBackForwardTransitionFallbackUXFaviconBackgroundColor(
          env, obj);
  jint page_background =
      Java_WebContentsDelegateAndroid_getBackForwardTransitionFallbackUXPageBackgroundColor(
          env, obj);
  return {
      .rounded_rectangle_color =
          SkColor4f::FromColor(static_cast<SkColor>(favicon_background)),
      .background_color =
          SkColor4f::FromColor(static_cast<SkColor>(page_background)),
  };
}

void WebContentsDelegateAndroid::ContentsZoomChange(bool zoom_in) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsDelegateAndroid_contentsZoomChange(env, obj, zoom_in);
}

content::NavigationController::UserAgentOverrideOption
WebContentsDelegateAndroid::ShouldOverrideUserAgentForPreloading(
    const GURL& url) {
  // Killswitch
  if (!base::FeatureList::IsEnabled(
          features::kPreloadingRespectUserAgentOverride)) {
    return WebContentsDelegate::ShouldOverrideUserAgentForPreloading(url);
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    // Fallback to base class version when JNI is unavailable.
    return WebContentsDelegate::ShouldOverrideUserAgentForPreloading(url);
  }

  ScopedJavaLocalRef<jobject> j_url =
      url::GURLAndroid::FromNativeGURL(env, url);
  int j_override_option =
      Java_WebContentsDelegateAndroid_shouldOverrideUserAgentForPreloading(
          env, obj, j_url);
  return static_cast<content::NavigationController::UserAgentOverrideOption>(
      j_override_option);
}

}  // namespace web_contents_delegate_android

DEFINE_JNI(WebContentsDelegateAndroid)
