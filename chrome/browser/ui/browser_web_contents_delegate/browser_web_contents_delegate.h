// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WEB_CONTENTS_DELEGATE_BROWSER_WEB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_WEB_CONTENTS_DELEGATE_BROWSER_WEB_CONTENTS_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserUiController;
class BrowserWindow;
class BrowserWindowInterface;
class DesktopBrowserWindowCapabilities;
class ExclusiveAccessManager;
class UnloadController;

namespace chrome {
class BrowserCommandController;
}

namespace web_app {
class AppBrowserController;
}

// This class handles the WebContentsDelegate responsibilities of its host
// browser.
class BrowserWebContentsDelegate : public content::WebContentsDelegate {
 public:
  DECLARE_USER_DATA(BrowserWebContentsDelegate);

  BrowserWebContentsDelegate(
      BrowserWindowInterface* browser,
      ExclusiveAccessManager& exclusive_access_manager,
      chrome::BrowserCommandController& command_controller,
      UnloadController& unload_controller,
      web_app::AppBrowserController* app_browser_controller,
      BrowserWindow& window,
      DesktopBrowserWindowCapabilities& capabilities,
      BrowserUiController& browser_ui_controller);
  BrowserWebContentsDelegate(const BrowserWebContentsDelegate&) = delete;
  BrowserWebContentsDelegate& operator=(const BrowserWebContentsDelegate&) =
      delete;
  ~BrowserWebContentsDelegate() override;

  static BrowserWebContentsDelegate* From(BrowserWindowInterface* browser);
  static const BrowserWebContentsDelegate* From(
      const BrowserWindowInterface* browser);

  // content::WebContentsDelegate:
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void RequestPointerLock(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void LostPointerLock() override;
  bool IsWaitingForPointerLockPrompt(
      content::WebContents* web_contents) override;
  bool AllowKeyboardLockForInnerContents(
      content::WebContents* web_contents) override;
  void RequestKeyboardLock(content::WebContents* web_contents,
                           bool esc_key_locked) override;
  void CancelKeyboardLockRequest(content::WebContents* web_contents) override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  int GetTopControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize(
      content::WebContents* contents) override;
  int GetVirtualKeyboardHeight(content::WebContents* contents) override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  bool CanOverscrollContent() override;
  bool ShouldPreserveAbortedURLs(content::WebContents* source) override;
  void SetFocusToLocationBar() override;
  void PreHandleDragUpdate(const content::DropData& drop_data,
                           const gfx::PointF& client_pt) override;
  void PreHandleDragExit() override;
  void HandleDragEnded() override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  void CreateSmsPrompt(content::RenderFrameHost*,
                       const std::vector<url::Origin>&,
                       const std::string& one_time_code,
                       base::OnceClosure on_confirm,
                       base::OnceClosure on_cancel) override;
  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;
  void OnDidBlockNavigation(
      content::WebContents* web_contents,
      const GURL& blocked_url,
      const GURL& initiator_url,
      const url::Origin& initiator_origin,
      blink::mojom::NavigationBlockedReason reason) override;
  bool IsBackForwardCacheSupported(content::WebContents& web_contents) override;

  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents,
      content::PreloadingTriggerType trigger_type) override;
  bool ShouldShowStaleContentOnEviction(content::WebContents* source) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void VisibleSecurityStateChanged(content::WebContents* source) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void ActivateContents(content::WebContents* contents) override;
  bool IsContentsActive(content::WebContents* contents) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;
  void CloseContents(content::WebContents* source) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  void ContentsMouseEvent(content::WebContents* source,
                          const ui::Event& event) override;
  void ContentsZoomChange(bool zoom_in) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  void BeforeUnloadFired(content::WebContents* source,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
  bool ShouldFocusPageAfterCrash(content::WebContents* source) override;
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
  bool ShouldUseInstancedSystemMediaControls() const override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;
  std::vector<blink::mojom::RelatedApplicationPtr> GetSavedRelatedApplications(
      content::WebContents* web_contents) override;
  content::WebContents* GetResponsibleWebContents(
      content::WebContents* web_contents) override;
  std::optional<gfx::Rect> GetWindowBoundsInScreen() override;
  bool IsWebContentsCreationOverridden(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          const content::GlobalRenderFrameHostId& opener_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void RendererResponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  bool GuestSaveFrame(content::WebContents* guest_web_contents) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void EnumerateDirectory(content::WebContents* web_contents,
                          scoped_refptr<content::FileSelectListener> listener,
                          const base::FilePath& path) override;
  bool GetCanResize() override;
  bool CanUseWindowingControls(
      content::RenderFrameHost* requesting_frame) override;
  void MinimizeFromWebAPI() override;
  void MaximizeFromWebAPI() override;
  void RestoreFromWebAPI() override;
  void SetResizableFromWebAPI(bool resizable) override;
  ui::mojom::WindowShowState GetWindowShowState() const override;
  bool CanEnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  content::FullscreenState GetFullscreenState(
      const content::WebContents* web_contents) const override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel(
      content::RenderFrameHost* requesting_frame) override;
  void RegisterProtocolHandler(content::RenderFrameHost* requesting_frame,
                               const std::string& protocol,
                               const GURL& url,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(content::RenderFrameHost* requesting_frame,
                                 const std::string& protocol,
                                 const GURL& url,
                                 bool user_gesture) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  void ProcessSelectAudioOutput(
      const content::SelectAudioOutputRequest& request,
      content::SelectAudioOutputCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  std::string GetTitleForMediaControls(
      content::WebContents* web_contents) override;
  void GetAIPageContent(
      content::WebContents* web_contents,
      bool include_actionable_elements,
      base::OnceCallback<void(const std::string&)> callback) override;
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;
  void CapturePaintPreviewOfSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      content::RenderFrameHost* render_frame_host) override;

 private:
  const base::ElapsedTimer creation_timer_;

  const raw_ref<ExclusiveAccessManager> exclusive_access_manager_;
  const raw_ref<chrome::BrowserCommandController> command_controller_;
  const raw_ref<UnloadController> unload_controller_;
  const raw_ptr<web_app::AppBrowserController> app_browser_controller_;
  const raw_ref<BrowserWindow> window_;
  const raw_ref<DesktopBrowserWindowCapabilities> capabilities_;
  const raw_ref<BrowserUiController> browser_ui_controller_;
  const raw_ref<BrowserWindowInterface> browser_;
  ui::ScopedUnownedUserData<BrowserWebContentsDelegate> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WEB_CONTENTS_DELEGATE_BROWSER_WEB_CONTENTS_DELEGATE_H_
