// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_BRIDGE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/app_shim/immersive_mode_controller_cocoa.h"
#include "components/remote_cocoa/app_shim/immersive_mode_tabbed_controller_cocoa.h"
#import "components/remote_cocoa/app_shim/mouse_capture_delegate.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_fullscreen_controller.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom-shared.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "components/remote_cocoa/common/text_input_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"
#include "ui/base/cocoa/command_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

@class BridgedContentView;
@class ModalShowAnimationWithLayer;
@class NativeWidgetMacNSWindow;
@class ViewsNSWindowDelegate;

namespace views::test {
class BridgedNativeWidgetTestApi;
}  // namespace views::test

namespace remote_cocoa {

namespace mojom {
class NativeWidgetNSWindowHost;
class TextInputHost;
}  // namespace mojom

class NativeWidgetNSWindowHostHelper;
class CocoaMouseCapture;
class CocoaWindowMoveLoop;
class DragDropClient;

using remote_cocoa::mojom::NativeWidgetNSWindowHost;
using remote_cocoa::NativeWidgetNSWindowHostHelper;
using remote_cocoa::CocoaMouseCapture;
using remote_cocoa::CocoaMouseCaptureDelegate;

// A bridge to an NSWindow managed by an instance of NativeWidgetMac or
// DesktopNativeWidgetMac. Serves as a helper class to bridge requests from the
// NativeWidgetMac to the Cocoa window. Behaves a bit like an aura::Window.
class REMOTE_COCOA_APP_SHIM_EXPORT NativeWidgetNSWindowBridge
    : public remote_cocoa::mojom::NativeWidgetNSWindow,
      public display::DisplayObserver,
      public NativeWidgetNSWindowFullscreenController::Client,
      public ui::CATransactionCoordinator::PreCommitObserver,
      public CocoaMouseCaptureDelegate {
 public:
  // Return the size that |window| will take for the given client area |size|,
  // based on its current style mask.
  static gfx::Size GetWindowSizeForClientSize(NSWindow* window,
                                              const gfx::Size& size);

  // Retrieve a NativeWidgetNSWindowBridge* from its id or window.
  static NativeWidgetNSWindowBridge* GetFromId(
      uint64_t bridged_native_widget_id);
  static NativeWidgetNSWindowBridge* GetFromNativeWindow(
      gfx::NativeWindow window);

  // Create an NSWindow for the specified parameters.
  static NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params);

  // Creates one side of the bridge. |host| and |parent| must not be NULL.
  NativeWidgetNSWindowBridge(
      uint64_t bridged_native_widget_id,
      NativeWidgetNSWindowHost* host,
      NativeWidgetNSWindowHostHelper* host_helper,
      remote_cocoa::mojom::TextInputHost* text_input_host);

  NativeWidgetNSWindowBridge(const NativeWidgetNSWindowBridge&) = delete;
  NativeWidgetNSWindowBridge& operator=(const NativeWidgetNSWindowBridge&) =
      delete;

  ~NativeWidgetNSWindowBridge() override;

  // Bind |bridge_mojo_receiver_| to |receiver|, and set the connection error
  // callback for |bridge_mojo_receiver_| to |connection_closed_callback| (which
  // will delete |this| when the connection is closed).
  void BindReceiver(mojo::PendingAssociatedReceiver<
                        remote_cocoa::mojom::NativeWidgetNSWindow> receiver,
                    base::OnceClosure connection_closed_callback);

  // Initialize the NSWindow. TODO(ccameron): When a NativeWidgetNSWindowBridge
  // is allocated across a process boundary, it will not be possible to
  // explicitly set an NSWindow in this way.
  void SetWindow(NativeWidgetMacNSWindow* window);

  // Set the command dispatcher delegate for the window. This will retain
  // |delegate| for the lifetime of |this|.
  void SetCommandDispatcher(
      NSObject<CommandDispatcherDelegate>* delegate,
      id<UserInterfaceItemCommandHandler> command_handler);

  // Start moving the window, pinned to the mouse cursor, and monitor events.
  // Return true on mouse up or false on premature termination via EndMoveLoop()
  // or when window is destroyed during the drag.
  bool RunMoveLoop(const gfx::Vector2d& drag_offset);
  void EndMoveLoop();

  // Sets the cursor associated with the NSWindow. Retains |cursor|.
  void SetCursor(NSCursor* cursor);

  // Called internally by the NSWindowDelegate when the window is closing.
  void OnWindowWillClose();

  // Called by the NSWindowDelegate when the size of the window changes.
  void OnSizeChanged();

  // Called once by the NSWindowDelegate when the position of the window has
  // changed.
  void OnPositionChanged();

  // Called by the NSWindowDelegate when the visibility of the window may have
  // changed. For example, due to a (de)miniaturize operation, or the window
  // being reordered in (or out of) the screen list.
  void OnVisibilityChanged();

  // Called by the NSWindowDelegate when the system colors change.
  void OnSystemColorsChanged();

  // Called by the NSWindowDelegate on screen, scale, or color space changes.
  void OnScreenOrBackingPropertiesChanged();

  // Called by the NSWindowDelegate when the window becomes or resigns key.
  void OnWindowKeyStatusChangedTo(bool is_key);

  // Called by the window show animation when it completes and wants to destroy
  // itself.
  void OnShowAnimationComplete();

  BridgedContentView* ns_view() { return bridged_view_; }
  NativeWidgetNSWindowHost* host() { return host_; }
  NativeWidgetNSWindowHostHelper* host_helper() { return host_helper_; }
  remote_cocoa::mojom::TextInputHost* text_input_host() const {
    return text_input_host_;
  }
  NSWindow* ns_window();

  remote_cocoa::DragDropClient* drag_drop_client();
  bool is_translucent_window() const { return is_translucent_window_; }

  // The parent widget specified in Widget::InitParams::parent. If non-null, the
  // parent will close children before the parent closes, and children will be
  // raised above their parent when window z-order changes.
  NativeWidgetNSWindowBridge* parent() { return parent_; }
  const std::vector<NativeWidgetNSWindowBridge*>& child_windows() {
    return child_windows_;
  }

  NativeWidgetNSWindowFullscreenController& fullscreen_controller() {
    return fullscreen_controller_;
  }
  bool target_fullscreen_state() const {
    return fullscreen_controller_.GetTargetFullscreenState();
  }
  bool window_visible() const;
  bool wants_to_be_visible() const { return wants_to_be_visible_; }
  bool in_fullscreen_transition() const {
    return fullscreen_controller_.IsInFullscreenTransition();
  }

  bool CanGoBack() const { return can_go_back_; }
  bool CanGoForward() const { return can_go_forward_; }

  // Whether to run a custom animation for the provided |transition|.
  bool ShouldRunCustomAnimationFor(
      remote_cocoa::mojom::VisibilityTransition transition) const;

  // Redispatch a keyboard event using the widget's window's CommandDispatcher.
  // Return true if the event is handled.
  bool RedispatchKeyEvent(NSEvent* event);

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // NativeWidgetNSWindowFullscreenController::Client:
  void FullscreenControllerTransitionStart(bool is_target_fullscreen) override;
  void FullscreenControllerTransitionComplete(bool is_fullscreen) override;
  void FullscreenControllerSetFrame(
      const gfx::Rect& frame,
      bool animate,
      base::OnceCallback<void()> completion_callback) override;
  void FullscreenControllerToggleFullscreen() override;
  void FullscreenControllerCloseWindow() override;
  int64_t FullscreenControllerGetDisplayId() const override;
  gfx::Rect FullscreenControllerGetFrameForDisplay(
      int64_t display_id) const override;
  gfx::Rect FullscreenControllerGetFrame() const override;

  // ui::CATransactionCoordinator::PreCommitObserver:
  bool ShouldWaitInPreCommit() override;
  base::TimeDelta PreCommitTimeout() override;

  // remote_cocoa::mojom::NativeWidgetNSWindow:
  void CreateWindow(mojom::CreateWindowParamsPtr params) override;
  void SetParent(uint64_t parent_id) override;
  void CreateSelectFileDialog(
      mojo::PendingReceiver<mojom::SelectFileDialog> receiver) override;
  void ShowCertificateViewer(
      const scoped_refptr<net::X509Certificate>& certificate) override;
  void StackAbove(uint64_t sibling_id) override;
  void StackAtTop() override;
  void ShowEmojiPanel() override;
  void InitWindow(
      remote_cocoa::mojom::NativeWidgetNSWindowInitParamsPtr params) override;
  void InitCompositorView(InitCompositorViewCallback callback) override;
  void CreateContentView(uint64_t ns_view_id, const gfx::Rect& bounds) override;
  void DestroyContentView() override;
  void CloseWindow() override;
  void CloseWindowNow() override;
  void SetInitialBounds(const gfx::Rect& new_bounds,
                        const gfx::Size& minimum_content_size) override;
  void SetBounds(const gfx::Rect& new_bounds,
                 const gfx::Size& minimum_content_size,
                 const std::optional<gfx::Size>& maximum_content_size) override;
  void SetSize(const gfx::Size& new_size,
               const gfx::Size& minimum_content_size) override;
  void SetSizeAndCenter(const gfx::Size& content_size,
                        const gfx::Size& minimum_content_size) override;
  void SetVisibilityState(
      remote_cocoa::mojom::WindowVisibilityState new_state) override;
  void SetAnimationEnabled(bool animation_enabled) override;
  void SetTransitionsToAnimate(
      remote_cocoa::mojom::VisibilityTransition transitions) override;
  void SetVisibleOnAllSpaces(bool always_visible) override;
  void SetZoomed(bool zoomed) override;
  void EnterFullscreen(int64_t target_display_id) override;
  void ExitFullscreen() override;
  void SetCanAppearInExistingFullscreenSpaces(
      bool can_appear_in_existing_fullscreen_spaces) override;
  void SetMiniaturized(bool miniaturized) override;
  void SetSizeConstraints(const gfx::Size& min_size,
                          const gfx::Size& max_size,
                          bool is_resizable,
                          bool is_maximizable) override;
  void SetOpacity(float opacity) override;
  void SetWindowLevel(int32_t level) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio,
                      const gfx::Size& excluded_margin) override;
  void SetCALayerParams(const gfx::CALayerParams& ca_layer_params) override;
  void SetWindowTitle(const std::u16string& title) override;
  void SetIgnoresMouseEvents(bool ignores_mouse_events) override;
  void MakeFirstResponder() override;
  void SortSubviews(
      const std::vector<uint64_t>& associated_subview_ids) override;
  void ClearTouchBar() override;
  void UpdateTooltip() override;
  void AcquireCapture() override;
  void ReleaseCapture() override;
  void RedispatchKeyEvent(
      const std::vector<uint8_t>& native_event_data) override;
  void SetLocalEventMonitorEnabled(bool enable) override;
  void SetCursor(const ui::Cursor& cursor) override;
  void EnableImmersiveFullscreen(uint64_t fullscreen_overlay_widget_id,
                                 uint64_t tab_widget_id) override;
  void DisableImmersiveFullscreen() override;
  void UpdateToolbarVisibility(
      remote_cocoa::mojom::ToolbarVisibilityStyle style) override;
  void OnTopContainerViewBoundsChanged(const gfx::Rect& bounds) override;
  void ImmersiveFullscreenRevealLock() override;
  void ImmersiveFullscreenRevealUnlock() override;
  void SetCanGoBack(bool can_go_back) override;
  void SetCanGoForward(bool can_go_back) override;
  void DisplayContextMenu(mojom::ContextMenuPtr menu,
                          mojo::PendingRemote<mojom::MenuHost> host,
                          mojo::PendingReceiver<mojom::Menu> receiver) override;
  void SetAllowScreenshots(bool allow) override;

  // Return true if [NSApp updateWindows] needs to be called after updating the
  // TextInputClient.
  bool NeedsUpdateWindows();

  // Compute the window and content size, and forward them to |host_|. This will
  // update widget and compositor size.
  void UpdateWindowGeometry();

  bool ShouldUseCustomTitlebarHeightForFullscreen() const;

  // Called by the ImmersiveModeController when the toolbar reveal status
  // changes. Note that the toolbar may be revealed while the menubar is hidden,
  // e.g. when "Always Show Toolbar in Full Screen" is enabled or there're
  // reveal locks.
  void OnImmersiveFullscreenToolbarRevealChanged(bool is_revealed);

  // Called by the ImmersiveModeController when the menubar reveal status
  // changes. `reveal_amount` ranges in [0, 1]. This is the opacity of the
  // menubar and the browser window traffic lights.
  void OnImmersiveFullscreenMenuBarRevealChanged(float reveal_amount);

  BOOL ImmersiveFullscreenEnabled() { return !!immersive_mode_controller_; }

  // Called by the ImmersiveModeController at the end of fullscreen transition
  // with the height of the menu bar if it autohides, or 0 if it doesn't.
  void OnAutohidingMenuBarHeightChanged(int menu_bar_height);

 private:
  friend class views::test::BridgedNativeWidgetTestApi;

  // Attach child windows, if the window is visible (see comment inline).
  void OrderChildren();

  // Closes all child windows. NativeWidgetNSWindowBridge children will be
  // destroyed.
  void RemoveOrDestroyChildren();

  // Remove the specified child window without closing it.
  void RemoveChildWindow(NativeWidgetNSWindowBridge* child);

  // Check if the window's zoomed state has changed. If changes happen, notify
  // the clients.
  void CheckAndNotifyZoomedStateChanged();

  // Notify descendants of a visibility change.
  void NotifyVisibilityChangeDown();

  // Query the display properties of the monitor that |window_| is on, and
  // forward them to |host_|.
  void UpdateWindowDisplay();

  // Return true if the delegate's modal type is window-modal. These display as
  // a native window "sheet", and have a different lifetime to regular windows.
  bool IsWindowModalSheet() const;

  // Show the window using -[NSApp beginSheet:..], modal for the parent window.
  void ShowAsModalSheet();

  // Returns true if capture exists and is currently active.
  bool HasCapture();

  // Returns true if window restoration data exists from session restore.
  bool HasWindowRestorationData();

  // CocoaMouseCaptureDelegate:
  bool PostCapturedEvent(NSEvent* event) override;
  void OnMouseCaptureLost() override;
  NSWindow* GetWindow() const override;

  const uint64_t id_;
  const raw_ptr<NativeWidgetNSWindowHost> host_;  // Weak. Owns this.
  const raw_ptr<NativeWidgetNSWindowHostHelper>
      host_helper_;  // Weak, owned by |host_|.
  const raw_ptr<remote_cocoa::mojom::TextInputHost>
      text_input_host_;  // Weak, owned by |host_|.

  NativeWidgetMacNSWindow* __strong window_;
  ViewsNSWindowDelegate* __strong window_delegate_;
  NSObject<CommandDispatcherDelegate>* window_command_dispatcher_delegate_;

  BridgedContentView* __strong bridged_view_;
  std::unique_ptr<remote_cocoa::ScopedNSViewIdMapping> bridged_view_id_mapping_;
  ModalShowAnimationWithLayer* __strong show_animation_;
  std::unique_ptr<CocoaMouseCapture> mouse_capture_;
  std::unique_ptr<CocoaWindowMoveLoop> window_move_loop_;
  ui::mojom::ModalType modal_type_ = ui::mojom::ModalType::kNone;
  bool is_translucent_window_ = false;
  id __strong key_down_event_monitor_;

  raw_ptr<NativeWidgetNSWindowBridge> parent_ =
      nullptr;  // Weak. If non-null, owns this.
  std::vector<NativeWidgetNSWindowBridge*> child_windows_;

  // The size of the content area of the window most recently sent to |host_|
  // (and its compositor).
  gfx::Size content_dip_size_;

  // The size of the frame most recently *received from* the compositor. Note
  // that during resize (and showing new windows), this will lag behind
  // |content_dip_size_|, which is the frame size most recently *sent to* the
  // compositor.
  gfx::Size compositor_frame_dip_size_;
  std::unique_ptr<ui::DisplayCALayerTree> display_ca_layer_tree_;

  // Tracks the bounds when the window last started entering fullscreen. Used to
  // provide an answer for GetRestoredBounds(), but not ever sent to Cocoa (it
  // has its own copy, but doesn't provide access to it).
  gfx::Rect bounds_before_fullscreen_;

  // The transition types to animate when not relying on native NSWindow
  // animation behaviors.
  remote_cocoa::mojom::VisibilityTransition transitions_to_animate_ =
      remote_cocoa::mojom::VisibilityTransition::kBoth;

  // Manager of fullscreen state transitions.
  NativeWidgetNSWindowFullscreenController fullscreen_controller_{this};

  // Stores the value last read from -[NSWindow isVisible], to detect visibility
  // changes.
  bool window_visible_ = false;

  // Stores the value last read from -[NSWindow isZoomed], to detect zoomed
  // state changes.
  bool window_zoomed_ = false;

  // If true, the window is either visible, or wants to be visible but is
  // currently hidden due to having a hidden parent.
  bool wants_to_be_visible_ = false;

  // If true, then ignore interactions with CATransactionCoordinator until the
  // first frame arrives.
  bool ca_transaction_sync_suppressed_ = false;

  // If true, the window has been made visible or changed shape and the window
  // shadow needs to be invalidated when a frame is received for the new shape.
  bool invalidate_shadow_on_frame_swap_ = false;

  // A blob representing the window's saved state, which is applied and cleared
  // on the first call to SetVisibilityState().
  std::vector<uint8_t> pending_restoration_data_;

  // Manages immersive mode when in fullscreen.
  std::unique_ptr<ImmersiveModeControllerCocoa> immersive_mode_controller_;

  // This tracks headless window visibility and fullscreen states.
  // In headless mode the platform window is never made visible or change its
  // state, so this structure holds the requested state for reporting.
  struct HeadlessModeWindow {
    bool visibility_state = false;
    bool fullscreen_state = false;
  };

  // This is present iff the window has been created in headless mode.
  std::optional<HeadlessModeWindow> headless_mode_window_;

  // This tracks whether current window can go back or go forward.
  bool can_go_back_ = false;
  bool can_go_forward_ = false;

  display::ScopedDisplayObserver display_observer_{this};

  mojo::AssociatedReceiver<remote_cocoa::mojom::NativeWidgetNSWindow>
      bridge_mojo_receiver_{this};

  // Keep track of ImmersiveFullscreenRevealLock() and
  // ImmersiveFullscreenRevealUnlock() calls so locks can persist across
  // immersive_mode_controller_ resets.
  int immersive_fullscreen_reveal_lock_count_ = 0;

  base::WeakPtrFactory<NativeWidgetNSWindowBridge> factory_{this};
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_BRIDGE_H_
