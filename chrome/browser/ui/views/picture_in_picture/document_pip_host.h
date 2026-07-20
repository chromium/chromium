// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window.h"
#include "chrome/browser/ui/views/picture_in_picture/pip_child_dialog_observer_helper.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

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
                        public PictureInPictureWindow,
                        public web_modal::WebContentsModalDialogManagerDelegate,
                        public web_modal::WebContentsModalDialogHost,
                        public views::WidgetObserver,
                        public views::ViewObserver,
                        public PipChildDialogObserverHelper::Delegate {
 public:
  DocumentPipHost(const DocumentPipHost&) = delete;
  DocumentPipHost& operator=(const DocumentPipHost&) = delete;

  ~DocumentPipHost() override;

  // Creates the PiP widget for the given child WebContents and shows it. Can be
  // called multiple times over the host's lifetime — each call opens a new PiP
  // window after the previous one has been closed via ClosePipWindow(). The
  // child WebContents ownership is transferred to the widget's WebView.
  // `initial_bounds` is the outer window bounds in screen coordinates,
  // pre-computed for the opener's display so the window opens on the same
  // monitor as the opener.
  void CreateAndShowPipWindow(
      std::unique_ptr<content::WebContents> child_web_contents,
      blink::mojom::PictureInPictureWindowOptions pip_options,
      const gfx::Rect& initial_bounds);

  // Tears down the currently open PiP widget (and its child WebContents). The
  // host stays attached to the opener WebContents so a later
  // CreateAndShowPipWindow() call can open a new window. Safe to call when no
  // widget is open.
  void Close();

  // Returns a WeakPtr to this host. The PictureInPictureWindowManager holds one
  // so it can drive/close the standalone window without owning the host.
  base::WeakPtr<DocumentPipHost> GetWeakPtr();

  // Accessors.
  Profile* GetProfile();
  content::WebContents* GetOpenerWebContents();
  content::WebContents* GetChildWebContents();
  views::Widget* GetWidget();
  const blink::mojom::PictureInPictureWindowOptions& GetPipOptions() const;

  // Looks up the DocumentPipHost that owns `child_web_contents` (the
  // WebContents rendered inside a Document PiP window), or nullptr if it is not
  // a Document PiP child. Used by the content-settings refresh path, which is
  // handed the captured (child) WebContents.
  static DocumentPipHost* FromChildWebContents(
      content::WebContents* child_web_contents);

  // Refreshes the camera/microphone content-setting icons in the PiP window's
  // title bar. No-op if the PiP window is not currently open.
  void UpdateContentSettingsIcons();

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
                          const content::GlobalRenderFrameHostId& opener_id,
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

  // content::WebContentsDelegate - Media:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  // PictureInPictureWindow:
  void SetForcedTucking(bool tuck) override;
#if BUILDFLAG(IS_MAC)
  void OnAnyBrowserEnteredFullscreen() override;
#endif

  // web_modal::WebContentsModalDialogManagerDelegate:
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;
  void NotifyPositionRequiresUpdate() override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  friend class content::WebContentsUserData<DocumentPipHost>;
  // Grant the child-dialog resize tests access to the private test-only hooks
  // below (RunPendingChildResizeForTesting/IsChildResizePendingForTesting), so
  // they can drive the pending resize deterministically without a public API.
  FRIEND_TEST_ALL_PREFIXES(DocumentPipFrameViewTest,
                           ChildDialogObserverResizesAndRestores);
  FRIEND_TEST_ALL_PREFIXES(DocumentPipDialogManagerDelegateTest,
                           ResizesPipToContainDialogThenRestores);

  // Private constructor called by WebContentsUserData machinery via
  // CreateForWebContents().
  explicit DocumentPipHost(content::WebContents* opener_web_contents);

  // Creates the WebContents helpers a standalone PiP child needs. The child is
  // not a tab, so TabHelpers never runs for it; this wires up the specific
  // helpers (PermissionRequestManager, TabModalDialogManager) directly. Must be
  // called after `widget_` is initialized, since the dialog manager anchors to
  // it.
  void CreateChildWebContentsHelpers(content::WebContents* child_web_contents);

  // Tears down the PiP widget (and with it the child WebContents, which is
  // owned by the WebView inside the widget's contents view). Safe to call
  // multiple times; subsequent calls are no-ops.
  void ClosePipWindow();

  // Callback for Widget::MakeCloseSynchronous(). Invoked when external code
  // (e.g. DialogDelegate, OS close button) requests the widget to close.
  void OnWidgetCloseRequested(views::Widget::ClosedReason reason);

  // Test-only hooks for the child-dialog resize path, reached by the friended
  // tests above. Private (not a public API); defined in the .cc where
  // ChildDialogObserverHelper is complete.
  bool IsChildResizePendingForTesting() const;
  void RunPendingChildResizeForTesting();

  // PipChildDialogObserverHelper::Delegate:
  views::Widget* GetPipWidget() override;
  gfx::Size ComputeDialogPadding() const override;
  void PositionChildDialog(views::Widget* child_dialog) override;
  // Re-applies the current tuck/untuck state to the PiP widget. Called by the
  // helper after it resizes the window for a child dialog, so a forced-tucked
  // window stays tucked at its new bounds.
  void EnforceTucking() override;

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

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::ScopedObservation<views::View, views::ViewObserver>
      contents_view_observation_{this};
  base::ObserverList<web_modal::ModalDialogHostObserver>
      modal_dialog_host_observer_list_;

  // Resizes the PiP widget so child dialogs are not clipped, and restores the
  // pre-dialog size when they close. Created in CreateAndShowPipWindow() once
  // the widget exists and reset in ClosePipWindow() before the widget is
  // destroyed (it observes the widget). Declared after `widget_` so it is
  // destroyed before the widget it observes.
  std::unique_ptr<PipChildDialogObserverHelper> child_dialog_observer_helper_;

  base::WeakPtrFactory<DocumentPipHost> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_
