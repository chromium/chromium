// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_BRIDGE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include <memory>
#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "components/remote_cocoa/app_shim/mouse_capture_delegate.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "components/remote_cocoa/common/text_input_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"
#include "ui/base/cocoa/command_dispatcher.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/display/display_observer.h"

@class BridgedContentView;
@class ModalShowAnimationWithLayer;
@class NativeWidgetMacNSWindow;
@class ViewsNSWindowDelegate;

namespace views {
namespace test {
class BridgedNativeWidgetTestApi;
}  // namespace test
}  // namespace views

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
  static base::scoped_nsobject<NativeWidgetMacNSWindow> CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params);

  // Creates one side of the bridge. |host| and |parent| must not be NULL.
  NativeWidgetNSWindowBridge(
      uint64_t bridged_native_widget_id,
      NativeWidgetNSWindowHost* host,
      NativeWidgetNSWindowHostHelper* host_helper,
      remote_cocoa::mojom::TextInputHost* text_input_host);
  ~NativeWidgetNSWindowBridge() override;

  // Bind |bridge_mojo_receiver_| to |receiver|, and set the connection error
  // callback for |bridge_mojo_receiver_| to |connection_closed_callback| (which
  // will delete |this| when the connection is closed).
  void BindReceiver(mojo::PendingAssociatedReceiver<
                        remote_cocoa::mojom::NativeWidgetNSWindow> receiver,
                    base::OnceClosure connection_closed_callback);

  // Initialize the NSWindow by taking ownership of the specified object.
  // TODO(ccameron): When a NativeWidgetNSWindowBridge is allocated across a
  // process boundary, it will not be possible to explicitly set an NSWindow in
  // this way.
  void SetWindow(base::scoped_nsobject<NativeWidgetMacNSWindow> window);

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

  // Called by the NSWindowDelegate when a fullscreen operation begins. If
  // |target_fullscreen_state| is true, the target state is fullscreen.
  // Otherwise, a transition has begun to come out of fullscreen.
  void OnFullscreenTransitionStart(bool target_fullscreen_state);

  // Called when a fullscreen transition completes. If target_fullscreen_state()
  // does not match |actual_fullscreen_state|, a new transition will begin.
  void OnFullscreenTransitionComplete(bool actual_fullscreen_state);

  // Transition the window into or out of fullscreen. This will immediately
  // invert the value of target_fullscreen_state().
  void ToggleDesiredFullscreenState(bool async = false);

  // Called by the NSWindowDelegate when the size of the window changes.
  void OnSizeChanged();

  // Called once by the NSWindowDelegate when the position of the window has
  // changed.
  void OnPositionChanged();

  // Called by the NSWindowDelegate when the visibility of the window may have
  // changed. For example, due to a (de)miniaturize operation, or the window
  // being reordered in (or out of) the screen list.
  void OnVisibilityChanged();

  // Called by the NSWindowDelegate when the system control tint changes.
  void OnSystemControlTintChanged();

  // Called by the NSWindowDelegate on a scale factor or color space change.
  void OnBackingPropertiesChanged();

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

  bool target_fullscreen_state() const { return target_fullscreen_state_; }
  bool window_visible() const { return window_visible_; }
  bool wants_to_be_visible() const { return wants_to_be_visible_; }
  bool in_fullscreen_transition() const { return in_fullscreen_transition_; }

  // Whether to run a custom animation for the provided |transition|.
  bool ShouldRunCustomAnimationFor(
      remote_cocoa::mojom::VisibilityTransition transition) const;

  // Redispatch a keyboard event using the widget's window's CommandDispatcher.
  // Return true if the event is handled.
  bool RedispatchKeyEvent(NSEvent* event);

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // ui::CATransactionCoordinator::PreCommitObserver:
  bool ShouldWaitInPreCommit() override;
  base::TimeDelta PreCommitTimeout() override;

  // remote_cocoa::mojom::NativeWidgetNSWindow:
  void CreateWindow(mojom::CreateWindowParamsPtr params) override;
  void SetParent(uint64_t parent_id) override;
  void CreateSelectFileDialog(
      mojo::PendingReceiver<mojom::SelectFileDialog> receiver) override;
  void StackAbove(uint64_t sibling_id) override;
  void StackAtTop() override;
  void ShowEmojiPanel() override;
  void InitWindow(
      remote_cocoa::mojom::NativeWidgetNSWindowInitParamsPtr params) override;
  void InitCompositorView() override;
  void CreateContentView(uint64_t ns_view_id, const gfx::Rect& bounds) override;
  void DestroyContentView() override;
  void CloseWindow() override;
  void CloseWindowNow() override;
  void SetInitialBounds(const gfx::Rect& new_bounds,
                        const gfx::Size& minimum_content_size) override;
  void SetBounds(const gfx::Rect& new_bounds,
                 const gfx::Size& minimum_content_size) override;
  void SetSizeAndCenter(const gfx::Size& content_size,
                        const gfx::Size& minimum_content_size) override;
  void SetVisibilityState(
      remote_cocoa::mojom::WindowVisibilityState new_state) override;
  void SetAnimationEnabled(bool animation_enabled) override;
  void SetTransitionsToAnimate(
      remote_cocoa::mojom::VisibilityTransition transitions) override;
  void SetVisibleOnAllSpaces(bool always_visible) override;
  void SetFullscreen(bool fullscreen) override;
  void SetCanAppearInExistingFullscreenSpaces(
      bool can_appear_in_existing_fullscreen_spaces) override;
  void SetMiniaturized(bool miniaturized) override;
  void SetSizeConstraints(const gfx::Size& min_size,
                          const gfx::Size& max_size,
                          bool is_resizable,
                          bool is_maximizable) override;
  void SetOpacity(float opacity) override;
  void SetWindowLevel(int32_t level) override;
  void SetContentAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetCALayerParams(const gfx::CALayerParams& ca_layer_params) override;
  void SetWindowTitle(const base::string16& title) override;
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

  // Return true if [NSApp updateWindows] needs to be called after updating the
  // TextInputClient.
  bool NeedsUpdateWindows();

  // Compute the window and content size, and forward them to |host_|. This will
  // update widget and compositor size.
  void UpdateWindowGeometry();

  // The offset in screen pixels for positioning child windows owned by |this|.
  gfx::Vector2d GetChildWindowOffset() const;

 private:
  friend class views::test::BridgedNativeWidgetTestApi;

  // Attach child windows, if the window is visible (see comment inline).
  void OrderChildren();

  // Closes all child windows. NativeWidgetNSWindowBridge children will be
  // destroyed.
  void RemoveOrDestroyChildren();

  // Remove the specified child window without closing it.
  void RemoveChildWindow(NativeWidgetNSWindowBridge* child);

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

  // CocoaMouseCaptureDelegate:
  void PostCapturedEvent(NSEvent* event) override;
  void OnMouseCaptureLost() override;
  NSWindow* GetWindow() const override;

  const uint64_t id_;
  NativeWidgetNSWindowHost* const host_;  // Weak. Owns this.
  NativeWidgetNSWindowHostHelper* const
      host_helper_;  // Weak, owned by |host_|.
  remote_cocoa::mojom::TextInputHost* const
      text_input_host_;  // Weak, owned by |host_|.

  base::scoped_nsobject<NativeWidgetMacNSWindow> window_;
  base::scoped_nsobject<ViewsNSWindowDelegate> window_delegate_;
  base::scoped_nsobject<NSObject<CommandDispatcherDelegate>>
      window_command_dispatcher_delegate_;

  base::scoped_nsobject<BridgedContentView> bridged_view_;
  std::unique_ptr<remote_cocoa::ScopedNSViewIdMapping> bridged_view_id_mapping_;
  base::scoped_nsobject<ModalShowAnimationWithLayer> show_animation_;
  std::unique_ptr<CocoaMouseCapture> mouse_capture_;
  std::unique_ptr<CocoaWindowMoveLoop> window_move_loop_;
  ui::ModalType modal_type_ = ui::MODAL_TYPE_NONE;
  bool is_translucent_window_ = false;
  bool widget_is_top_level_ = false;
  bool position_window_in_screen_coords_ = false;

  NativeWidgetNSWindowBridge* parent_ =
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

  // Whether this window wants to be fullscreen. If a fullscreen animation is in
  // progress then it might not be actually fullscreen.
  bool target_fullscreen_state_ = false;

  // Whether this window is in a fullscreen transition, and the fullscreen state
  // can not currently be changed.
  bool in_fullscreen_transition_ = false;

  // Trying to close an NSWindow during a fullscreen transition will cause the
  // window to lock up. Use this to track if CloseWindow was called during a
  // fullscreen transition, to defer the -[NSWindow close] call until the
  // transition is complete.
  // https://crbug.com/945237
  bool has_deferred_window_close_ = false;

  // Stores the value last read from -[NSWindow isVisible], to detect visibility
  // changes.
  bool window_visible_ = false;

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
  // the first time it's shown.
  std::vector<uint8_t> pending_restoration_data_;

  mojo::AssociatedReceiver<remote_cocoa::mojom::NativeWidgetNSWindow>
      bridge_mojo_receiver_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetNSWindowBridge);
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_NS_WINDOW_BRIDGE_H_
