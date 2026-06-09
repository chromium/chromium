// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_

#include <optional>
#include <string>

#include "base/timer/elapsed_timer.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/views/widget/widget.h"

class DocumentPipWidgetDelegate;
class PictureInPictureTucker;
class Profile;

namespace content {
class WebContents;
}  // namespace content

// DocumentPipHost is the standalone Document Picture-in-Picture host.
// It is attached to the *opener* WebContents as a WebContentsUserData and:
//   - Owns the child WebContents for the PiP window.
//   - Owns the floating views::Widget that renders that child.
//   - Acts as WebContentsDelegate for the child WebContents.
//   - Observes the opener WebContents to close the PiP window when the opener
//     is destroyed or navigates to a new primary page.
//   - Implements PictureInPictureWindow for tucking and Mac fullscreen.
class DocumentPipHost : public content::WebContentsUserData<DocumentPipHost>,
                        public content::WebContentsObserver,
                        public content::WebContentsDelegate,
                        public PictureInPictureWindow {
 public:
  DocumentPipHost(const DocumentPipHost&) = delete;
  DocumentPipHost& operator=(const DocumentPipHost&) = delete;

  ~DocumentPipHost() override;

  // Creates the PiP widget for the given child WebContents. Can be called
  // multiple times over the host's lifetime - each call opens a new PiP window
  // after the previous one has been closed via ClosePipWindow(). The child
  // WebContents ownership is transferred to the widget's WebView.
  void CreatePipWidget(std::unique_ptr<content::WebContents> child_web_contents,
                       blink::mojom::PictureInPictureWindowOptions pip_options);

  // Accessors.
  Profile* GetProfile();
  content::WebContents* GetOpenerWebContents();
  content::WebContents* GetChildWebContents();
  views::Widget* GetWidget();
  const blink::mojom::PictureInPictureWindowOptions& GetPipOptions() const;

  // content::WebContentsObserver (observing the opener):
  // Bring WebContentsObserver::BeforeUnloadFired(bool) into scope so the
  // WebContentsDelegate::BeforeUnloadFired() override below does not hide it.
  using content::WebContentsObserver::BeforeUnloadFired;
  void PrimaryPageChanged(content::Page& page) override;

  // content::WebContentsDelegate - Navigation & State:
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  void CloseContents(content::WebContents* source) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;
  void VisibleSecurityStateChanged(content::WebContents* source) override;

  // content::WebContentsDelegate - Window Activation & Bounds:
  void ActivateContents(content::WebContents* contents) override;
  bool IsContentsActive(content::WebContents* contents) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;

  // content::WebContentsDelegate - UI Events & Input:
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  void ContentsMouseEvent(content::WebContents* source,
                          const ui::Event& event) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;

  // content::WebContentsDelegate - New Windows & Popups:
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  bool IsWebContentsCreationOverridden(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;

  // content::WebContentsDelegate - Dialogs & Logging:
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;

  // content::WebContentsDelegate - Window Properties & Fullscreen:
  bool GetCanResize() override;
  ui::mojom::WindowShowState GetWindowShowState() const override;
  content::FullscreenState GetFullscreenState(
      const content::WebContents* web_contents) const override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  bool CanEnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame) override;

  // content::WebContentsDelegate - Feature Capabilities:
  bool CanOverscrollContent() override;
  bool IsBackForwardCacheSupported(content::WebContents& web_contents) override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
  bool ShouldUseInstancedSystemMediaControls() const override;
  content::WebContents* GetResponsibleWebContents(
      content::WebContents* web_contents) override;
  std::string GetTitleForMediaControls(
      content::WebContents* web_contents) override;
  void UpdatePreferredSize(content::WebContents* web_contents,
                           const gfx::Size& pref_size) override;
  std::optional<gfx::Rect> GetWindowBoundsInScreen() override;
  void BeforeUnloadFired(content::WebContents* tab,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;

  // PictureInPictureWindow:
  void SetForcedTucking(bool tuck) override;
#if BUILDFLAG(IS_MAC)
  void OnAnyBrowserEnteredFullscreen() override;
#endif

 private:
  friend class content::WebContentsUserData<DocumentPipHost>;

  // Private constructor called by WebContentsUserData machinery via
  // CreateForWebContents().
  explicit DocumentPipHost(content::WebContents* opener_web_contents);

  // Tears down the PiP widget (and with it the child WebContents, which is
  // owned by the WebView inside the widget's contents view). Safe to call
  // multiple times; subsequent calls are no-ops.
  void ClosePipWindow();

  // Callback for Widget::MakeCloseSynchronous(). Invoked when external code
  // (e.g. DialogDelegate, OS close button) requests the widget to close.
  void OnWidgetCloseRequested(views::Widget::ClosedReason reason);

  // The delegate for the floating Widget. Owned by this host (not by the
  // Widget): `CLIENT_OWNS_WIDGET` + not `SetOwnedByWidget()` means the Widget
  // never deletes it, so the client must. Declared before `widget_` so it is
  // destroyed after the Widget, since the Widget references it via a raw
  // pointer set in Init().
  std::unique_ptr<DocumentPipWidgetDelegate> widget_delegate_;

  // The floating window hosting the PiP child WebContents.
  // Declared after `widget_delegate_` so it is destroyed first: members are
  // destroyed in reverse declaration order.
  std::unique_ptr<views::Widget> widget_;

  // Initial options from the requestWindow() call.
  blink::mojom::PictureInPictureWindowOptions pip_options_;

  // Tracks time since host creation, used by SetContentsBounds to record
  // kMovedOrResizedPopup2sAfterCreation - aligned with Browser's behavior.
  base::ElapsedTimer creation_timer_;

  // Manages tucking the PiP window offscreen. Created lazily on first
  // SetForcedTucking() call.
  std::unique_ptr<PictureInPictureTucker> tucker_;
  bool is_tucking_forced_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_
