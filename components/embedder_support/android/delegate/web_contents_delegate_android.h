// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include <stdint.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"

class GURL;

namespace content {
class WebContents;
class WebContentsDelegate;
class NavigationHandle;
struct NativeWebKeyboardEvent;
struct OpenURLParams;
}  // namespace content

namespace web_contents_delegate_android {

enum WebContentsDelegateLogLevel {
  // Equivalent of WebCore::WebConsoleMessage::LevelDebug.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG = 0,
  // Equivalent of WebCore::WebConsoleMessage::LevelLog.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_LOG = 1,
  // Equivalent of WebCore::WebConsoleMessage::LevelWarning.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_WARNING = 2,
  // Equivalent of WebCore::WebConsoleMessage::LevelError.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_ERROR = 3,
};

// Native underpinnings of WebContentsDelegateAndroid.java. Provides a default
// delegate for WebContents to forward calls to the java peer. The embedding
// application may subclass and override methods on either the C++ or Java side
// as required.
class WebContentsDelegateAndroid : public content::WebContentsDelegate {
 public:
  WebContentsDelegateAndroid(JNIEnv* env,
                             const jni_zero::JavaRef<jobject>& obj);
  ~WebContentsDelegateAndroid() override;

  // Overridden from WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  std::unique_ptr<content::ColorChooser> OpenColorChooser(
      content::WebContents* source,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void VisibleSecurityStateChanged(content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void RendererResponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  void CloseContents(content::WebContents* source) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  bool ShouldBlockMediaRequest(const GURL& url) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void FullscreenStateChangedForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  void OnDidBlockNavigation(
      content::WebContents* web_contents,
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) override;
  int GetTopControlsHeight() override;
  int GetTopControlsMinHeight() override;
  int GetBottomControlsHeight() override;
  int GetBottomControlsMinHeight() override;
  bool ShouldAnimateBrowserControlsHeightChanges() override;
  bool DoBrowserControlsShrinkRendererSize(
      content::WebContents* contents) override;
  int GetVirtualKeyboardHeight(content::WebContents* contents) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  void DidChangeCloseSignalInterceptStatus() override;
  // Return true if the WebContents is presenting a java native view for the
  // committed navigation entry. This is possible for chrome* URLs, such as
  // an NTP. Callback is guaranteed to be dispatched asynchronously (with an
  // empty bitmap if the capture fails) only if this returns true.
  bool MaybeCopyContentAreaAsBitmap(
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  SkBitmap MaybeCopyContentAreaAsBitmapSync() override;
  void DidBackForwardTransitionAnimationChange() override;
  content::BackForwardTransitionAnimationManager::FallbackUXConfig
  GetBackForwardTransitionFallbackUXConfig() override;
  void ContentsZoomChange(bool zoom_in) override;

 protected:
  base::android::ScopedJavaLocalRef<jobject> GetJavaDelegate(JNIEnv* env) const;

 private:
  // We depend on the java side user of WebContentDelegateAndroid to hold a
  // strong reference to that object as long as they want to receive callbacks
  // on it. Using a weak ref here allows it to be correctly GCed.
  JavaObjectWeakGlobalRef weak_java_delegate_;
};

}  // namespace web_contents_delegate_android

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_WEB_CONTENTS_DELEGATE_ANDROID_H_
