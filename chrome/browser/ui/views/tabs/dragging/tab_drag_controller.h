// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <tuple>
#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/dragging_tabs_session.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#endif  // defined(USE_AURA)

namespace ui {
class ListSelectionModel;
class PresentationTimeRecorder;
}  // namespace ui
namespace views {
class View;
class ViewTracker;
}  // namespace views
namespace viz {
struct CopyOutputBitmapWithMetadata;
}  // namespace viz
namespace tabs {
class TabModel;
}
class Browser;
class EventTracker;
class Tab;
class TabDragControllerTest;
class TabDragContext;
class TabSlotView;
class TabStripModel;
class TabStripScrollSession;
class WindowFinder;
class TabStripScrollSession;
struct DetachedTabCollection;
struct DetachedTab;

// `TabDragDelegate` is an interface that may be implemented to facilitate
// custom behavior beyond the tabstrip.
// TODO(crbug.com/394370034): The tabstrip currently has logic that is a
// good candidate for being a `TabDragDelegate`, but is tightly coupled with
// responsibilities related to `TabDragContext` lifetime management. We should
// attempt to split out as much of this logic as possible into a new
// `TabDragDelegate`.
class TabDragDelegate {
 public:
  // An interface exposed to TabDragDelegate, allowing interaction with the
  // ongoing drag session.
  class DragController {
   public:
    virtual ~DragController() = default;

    // Detaches the tab corresponding to the index within the current
    // `DragSessionData`. If this is the last tab in the browser, the browser
    // will close.
    // This can only be called once dragging stopped and the referenced
    // tab data must not have been already destroyed.
    virtual std::unique_ptr<tabs::TabModel> DetachTabAtForInsertion(
        int drag_idx) = 0;
    virtual const DragSessionData& GetSessionData() const = 0;
  };

  virtual ~TabDragDelegate() = default;

  // Invoked when this becomes the delegate of the drag controller.
  virtual void OnTabDragEntered() = 0;

  // Invoked on each iteration of the drag loop where this is the delegate of
  // the drag controller.
  virtual void OnTabDragUpdated(TabDragDelegate::DragController& controller,
                                const gfx::Point& point_in_screen) = 0;

  // Invoked when this delegate is no longer targeted by the controller.
  virtual void OnTabDragExited() = 0;

  // Notification for the end of a drag, for any reason (e.g. drop, cancel,
  // etc.).
  virtual void OnTabDragEnded() = 0;

  // Indicates whether this delegate should handle a dropped tab.
  virtual bool CanDropTab() = 0;

  // Handles a drop that occurred while this delegate is targeted.
  // This is only invoked if `CanDropTab` returned `true`.
  virtual void HandleTabDrop(DragController& controller) = 0;

  // Registers a callback that gets invoked when this is being destroyed.
  virtual base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback) = 0;
};

// An interface for fetching a `TabDragDelegate` from a given browser and
// point.
class TabDragPointResolver {
 public:
  virtual ~TabDragPointResolver() = default;
  virtual TabDragDelegate* GetDragTarget(BrowserView& browser_view,
                                         const gfx::Point& point_in_screen) = 0;
};

// TabDragController is responsible for managing the tab dragging session. When
// the user presses the mouse on a tab a new TabDragController is created and
// Drag() is invoked as the mouse is dragged. If the mouse is dragged far enough
// TabDragController starts a drag session. The drag session is completed when
// EndDrag() is invoked (or the TabDragController is destroyed).
//
// While dragging within a tab strip TabDragController sets the bounds of the
// tabs (this is referred to as attached). When the user drags far enough such
// that the tabs should be moved out of the tab strip a new Browser is created
// and RunMoveLoop() is invoked on the Widget to drag the browser around. This
// is the default on aura.
class TabDragController : public views::WidgetObserver,
                          public TabDragDelegate::DragController
#if defined(USE_AURA)
    ,
                          public aura::client::DragDropClientObserver
#endif  // defined(USE_AURA)
{
 public:
  // Amount above or below the tabstrip the user has to drag before detaching.
  static const int kTouchVerticalDetachMagnetism;
  static const int kVerticalDetachMagnetism;

  TabDragController();
  TabDragController(const TabDragController&) = delete;
  TabDragController& operator=(const TabDragController&) = delete;
  ~TabDragController() override;

  // Whether this TabDragController still exists - used as a return type for
  // methods which may end the drag session and thus destroy the
  // TabDragController. These methods should also be annotated with
  // [[nodiscard]] to force the caller to handle the case where the
  // TabDragController was destroyed.
  //
  // Note that, since TabDragController makes system calls in many places, and
  // many or most of those may reenter Chrome, the TabStrip, and the
  // TabDragController, it's generally not possible to make strong guarantees
  // about what can and cannot happen in various cases - code defensively.
  //
  // TODO(crbug.com/41482188): Return this from *all* methods which may end the
  // drag (but maybe skip the public notification methods, e.g. TabWasAdded).
  enum class Liveness {
    kAlive,
    kDeleted,
  };

  // Initializes TabDragController to drag the views in `dragging_views`
  // originating from `source_context`. `source_view` is the view that
  // initiated the drag and is either a Tab or a TabGroupHeader contained in
  // `dragging_views`. `mouse_offset` is the distance of the mouse pointer from
  // the origin of the first view in `dragging_views` and `source_view_offset`
  // the offset from `source_view`. `source_view_offset` is the horizontal
  // offset of `mouse_offset` relative to `source_view`.
  // `initial_selection_model` is the selection model before the drag started
  // and is only non-empty if the original selection isn't the same as the
  // dragging set. Returns Liveness::DELETED if `this` was deleted during this
  // call, and Liveness::ALIVE if `this` still exists.
  [[nodiscard]] Liveness Init(TabDragContext* source_context,
                              TabSlotView* source_view,
                              const std::vector<TabSlotView*>& dragging_views,
                              const gfx::Point& mouse_offset,
                              int source_view_offset,
                              ui::ListSelectionModel initial_selection_model,
                              ui::mojom::DragEventSource event_source);

  // Returns true if there is a drag underway and the drag is attached to
  // `tab_strip`.
  // NOTE: this returns false if the TabDragController is in the process of
  // finishing the drag.
  static bool IsAttachedTo(const TabDragContextBase* tab_strip);

  // Returns true if there is a drag underway.
  static bool IsActive();

  // Returns true if a regular drag and drop session is running as a fallback
  // instead of a move loop.
  static bool IsSystemDnDSessionRunning();

  // Called by TabStrip / TabStripRegionView to inform TabDragController.
  static void OnSystemDnDUpdated(const ui::DropTargetEvent& event);
  static void OnSystemDnDExited();
  static void OnSystemDnDEnded();

  // Returns the pointer of `source_context_`.
  static TabDragContext* GetSourceContext();

  ui::mojom::DragEventSource event_source() const { return event_source_; }

  // See description above fields for details on these.
  bool active() const { return current_state_ != DragState::kStopped; }
  const TabDragContext* attached_context() const { return attached_context_; }

  // Returns true if a drag started.
  bool started_drag() const { return current_state_ != DragState::kNotStarted; }

  // Returns the tab group being dragged, if any. Will only return a value if
  // the user is dragging a tab group header, not an individual tab or tabs
  // from a group.
  const std::optional<tab_groups::TabGroupId> group() const {
    return drag_data_.group();
  }

  bool IsMovingLastTab() const { return is_moving_last_tab_; }

  // Call when a tab was just added to the attached tabstrip. May end the drag.
  void TabWasAdded();

  // Call when `contents` is about to be removed from the attached tabstrip. May
  // end the drag.
  void OnTabWillBeRemoved(content::WebContents* contents);

  // Returns true if removing `contents` from the attached tabstrip is fine, and
  // false if that would be problematic for the drag session.
  bool CanRemoveTabDuringDrag(content::WebContents* contents) const;

  // Returns true if restoring a fullscreen window during a drag is allowed.
  bool CanRestoreFullscreenWindowDuringDrag() const;

  // Invoked to drag to the new location, in screen coordinates.
  [[nodiscard]] Liveness Drag(const gfx::Point& point_in_screen);

  // Complete the current drag session.
  void EndDrag(EndDragReason reason);

  // Set a callback to be called when the nested drag loop / system DnD session
  // finishes.
  //
  // The details of when this callback is called are as follows:
  // - If using a nested drag loop, it is called when dragging tabs into a
  //   browser; or when dropping a window.
  // - If using system DnD, it is called when releasing the mouse after having
  //   dragged out of the window at any point in time during the drag session;
  //   or when dragging tabs into a browser, if all tabs from the source
  //   browser were part of the drag and therefore the source browser is closed.
  //   Also, the callback must be set before the system DnD session starts.
  //   As that session keeps running until the end of the tab dragging session,
  //   this means that this method has no effect after entering the
  //   kDraggingUsingSystemDnD state for the first time.
  void SetDragLoopDoneCallbackForTesting(base::OnceClosure callback);

 private:
  friend class TabDragControllerTest;

  class SourceTabStripEmptinessTracker;

  class DraggedTabsClosedTracker;

  // Used to indicate the direction the mouse has moved when attached.
  static const int kMovedMouseLeft = 1 << 0;
  static const int kMovedMouseRight = 1 << 1;

  enum class DragState {
    // The drag has not yet started; the user has not dragged far enough to
    // begin a session.
    kNotStarted,
    // The session is dragging a set of tabs within `attached_context_`.
    kDraggingTabs,
    // The session is dragging a window; `attached_context_` is that window's
    // tabstrip.
    kDraggingWindow,
    // The platform does not support client controlled window dragging; instead,
    // a regular drag and drop session is running. The dragged tabs are still
    // moved to a new browser, but it stays hidden until the drag ends. On
    // platforms where this state is used, the kDraggingWindow and
    // kWaitingToDragTabs states are not used.
    kDraggingUsingSystemDnD,
    // The session has already attached to the target tabstrip, but must wait
    // for the nested move loop to exit to transition to kDraggingTabs. Used on
    // platforms where `can_release_capture_` is false.
    kWaitingToExitRunLoop,
    // The session is still attached to the drag-created window, and is waiting
    // for the nested move loop to exit to transition to kDraggingTabs and
    // attach to `tab_strip_to_attach_to_after_exit_`. Used on platforms where
    // `can_release_capture_` is true.
    kWaitingToDragTabs,
    // The drag session has completed or been canceled.
    kStopped
  };

  // Enumeration of the ways a drag session can end.
  enum class EndDragType {
    // Drag session exited normally: the user released the mouse.
    kNormal,

    // The drag session was canceled (alt-tab during drag, escape ...)
    kCanceled,

    // The tab (NavigationController) was destroyed during the drag.
    kTabDestroyed
  };

  // Whether Detach() should release capture or not.
  enum class ReleaseCapture {
    kReleaseCapture,
    kDontReleaseCapture,
  };

  // Specifies what should happen when a drag motion exits the tab strip region
  // in an attempt to detach a tab.
  enum class DetachBehavior { kDetachable, kNotDetachable };

  // Overridden from views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  // TabDragDelegate::DragController
  std::unique_ptr<tabs::TabModel> DetachTabAtForInsertion(
      int drag_idx) override;
  const DragSessionData& GetSessionData() const override;

  // Forget the source tabstrip. It doesn't exist any more, so it doesn't
  // make sense to insert dragged tabs back into it if the drag is reverted.
  void OnSourceTabStripEmpty();

  // A tab was closed in the active tabstrip. Clean up if we were dragging it.
  void OnActiveStripWebContentsRemoved(content::WebContents* contents);

  // The WebContents in a tab was replaced in the active tabstrip. Update our
  // canonical reference if we were dragging that tab.
  void OnActiveStripWebContentsReplaced(content::WebContents* previous,
                                        content::WebContents* next);

  void UpdateDockInfo(const gfx::Point& point_in_screen);

  // Saves focus in the window that the drag initiated from. Focus will be
  // restored appropriately if the drag ends within this same window.
  [[nodiscard]] Liveness SaveFocus();

  // Restore focus to the View that had focus before the drag was started, if
  // the drag ends within the same Window as it began.
  void RestoreFocus();

  // Tests whether `point_in_screen` is past a minimum elasticity threshold
  // required to start a drag.
  bool CanStartDrag(const gfx::Point& point_in_screen) const;

  // Invoked once a drag has started to determine the appropriate context to
  // drag to (which may be the currently attached one).
  [[nodiscard]] Liveness ContinueDragging(const gfx::Point& point_in_screen);

  // Transitions dragging from `attached_context_` to `target_context`.
  // `target_context` is nullptr if the mouse is not over a valid tab strip.
  [[nodiscard]] Liveness DragBrowserToNewTabStrip(
      TabDragContext* target_context,
      const gfx::Point& point_in_screen);

  // Returns true if we should use a system drag and drop session when dragging
  // tabs outside of a tabstrip.
  bool ShouldDragWindowUsingSystemDnD();

  // Requests a tab thumbnail of the dragged tab to be used as a drag icon.
  void RequestTabThumbnail();

  // Stores a scaled version of `thumbnail` in `drag_image_`, and calls
  // UpdateSystemDnDDragImage() if we're currently dragging using system DnD.
  //
  // `window_scale` is the scale of the window that `thumbnail` was captured
  // from.
  void OnTabThumbnailAvailable(float window_scale,
                               const viz::CopyOutputBitmapWithMetadata& result);

  // Starts a regular drag and drop session as a fallback if RunMoveLoop() is
  // not supported and no drag session is currently running. `context` is used
  // to get the widget that will initiate the drag session, and can be NULL if
  // `system_drag_and_drop_session_running_` is true.
  [[nodiscard]] Liveness StartSystemDnDSessionIfNecessary(
      TabDragContext* context,
      gfx::Point point_in_screen);

  // Stored as a callback in `drag_started_callback_`. See the comment in
  // StartSystemDnDSessionIfNecessary() for more details.
  void HideAttachedContext();

  // Returns the compatible TabDragContext to drag to at the specified point
  // (screen coordinates), or nullptr if there is none. May end the drag on
  // some platforms as a result of reentrancy during system calls, hence this
  // also returns a Liveness.
  [[nodiscard]] std::tuple<Liveness, TabDragContext*, TabDragDelegate*>
  GetDragTargetForPoint(gfx::Point point_in_screen);

  // Returns true if `context` contains the specified point in screen
  // coordinates.
  bool DoesTabStripContain(TabDragContext* context,
                           const gfx::Point& point_in_screen) const;

  // Begin the drag session by attaching to `source_context_`.
  void StartDrag();

  // Insert the dragged tabs into `attached_context` and attach the drag session
  // to it. The newly attached context will have capture, and will take
  // ownership of `controller` (which must be `this`).
  void AttachToNewContext(
      TabDragContext* attached_context,
      std::unique_ptr<TabDragController> controller,
      std::vector<std::variant<std::unique_ptr<DetachedTab>,
                               std::unique_ptr<DetachedTabCollection>>>
          owned_tabs_and_collections);

  // Sets up dragging in `attached_context_`. The dragged tabs must already
  // be present.
  void AttachImpl();

  // Detach the dragged tabs from the current TabDragContext. Returns
  // ownership of the owned controller, which must be `this`, if
  // `attached_context_` currently owns a controller. Otherwise returns
  // nullptr.
  std::tuple<std::unique_ptr<TabDragController>,
             std::vector<std::variant<std::unique_ptr<DetachedTab>,
                                      std::unique_ptr<DetachedTabCollection>>>>
  Detach(ReleaseCapture release_capture);

  // Detach from `attached_context_` and attach to `target_context` instead.
  // See Detach/Attach for parameter documentation. Transfers ownership of
  // `this` from `attached_context_` (which must own `this`) to
  // `target_context`.
  void DetachAndAttachToNewContext(ReleaseCapture release_capture,
                                   TabDragContext* target_context);

  // Detaches the tabs being dragged, creates a new Browser to contain them and
  // runs a nested move loop.
  [[nodiscard]] Liveness DetachIntoNewBrowserAndRunMoveLoop(
      gfx::Point point_in_screen);

  // Runs a nested run loop that handles moving the current Browser.
  // `drag_offset` is the desired offset between the cursor and the window
  // origin. `point_in_screen` is the cursor location in screen space.
  [[nodiscard]] Liveness RunMoveLoop(gfx::Point point_in_screen,
                                     gfx::Vector2d drag_offset);

  // Finds the TabSlotViews within the specified TabDragContext that
  // corresponds to the WebContents of the dragged views. Also finds the group
  // header if it is dragging. Returns an empty vector if not attached.
  std::vector<TabSlotView*> GetViewsMatchingDraggedContents(
      TabDragContext* context);

  // Does the work for EndDrag(). If we actually started a drag and `how_end` is
  // not TAB_DESTROYED then one of CompleteDrag() or RevertDrag() is invoked.
  void EndDragImpl(EndDragType how_end);

  // Reverts a cancelled drag operation.
  void RevertDrag();

  // Selects the dragged tabs in `model`. Does nothing if there are no longer
  // any dragged contents (as happens when a WebContents is deleted out from
  // under us).
  void ResetSelection(TabStripModel* model);

  // Restores `initial_selection_model_` to the `source_context_`.
  void RestoreInitialSelection();

  // Reverts the drag the group starting at `drag_index_`.
  void RevertGroupAt(size_t drag_index);

  // Reverts the tab at `drag_index` in `drag_data_`.
  void RevertTabAt(size_t drag_index);

  // Reverts the split starting at `drag_index_`.
  void RevertSplitAt(size_t drag_index);

  // Finishes a successful drag operation.
  void CompleteDrag();

  // Maximizes the attached window.
  void MaximizeAttachedWindow();

  // Hides the frame for the window that contains the TabDragContext
  // the current drag session was initiated from.
  void HideFrame();

  void BringWindowUnderPointToFront(const gfx::Point& point_in_screen);

  [[nodiscard]] Liveness SetCapture(TabDragContext* context);

  // Returns true if currently dragging a tab with `contents`.
  bool IsDraggingTab(content::WebContents* contents) const;

  // Returns the Widget of the currently attached TabDragContext's
  // BrowserView.
  views::Widget* GetAttachedBrowserWidget();

  // Restores and resizes `attached_context_` so it can be dragged.
  void RestoreAttachedWindowForDrag();

  // Calculates and returns new size for the dragged browser window.
  // Takes into consideration current and restore bounds of `source` tab strip
  // preventing the dragged size from being too small.
  gfx::Size CalculateDraggedWindowSize(TabDragContext* source);

  // Calculates how tabs should be positioned in a to-be-detached window based
  // on how the window is being detached. Returns the leading x coordinate of
  // the leading-most dragged view.
  int GetTabOffsetForDetachedWindow(gfx::Point point_in_screen);

  // Positions the dragged tabs within `attached_context_` appropriately for
  // kDraggingWindow. When `previous_tab_area_width` is larger than the new tab
  // area width, the tabs are scaled and positioned to maintain a sense of
  // continuity.
  void AdjustTabBoundsForDrag(int previous_tab_area_width,
                              int first_tab_leading_x,
                              std::vector<gfx::Rect> drag_bounds);

  // If the user is dragging a single tab that is controlled by one web app,
  // and features::kTearOffWebAppTabOpensWebAppWindow is enabled,
  // returns the app id of that web app, nullopt otherwise.
  std::optional<webapps::AppId> GetControllingAppForDrag(Browser* browser);

  // Creates and returns a new Browser to handle the drag.
  Browser* CreateBrowserForDrag(TabDragContext* source, gfx::Size initial_size);

  // Returns the location of the cursor. This is either the location of the
  // mouse or the location of the current touch point.
  gfx::Point GetCursorScreenPoint();

  // Calculates the drag offset needed to place the correct point on the
  // source view under the cursor.
  gfx::Vector2d CalculateWindowDragOffset();

  // Returns the NativeWindow in `window` at the specified point. If
  // `exclude_dragged_view` is true, then the dragged view is not considered.
  [[nodiscard]] Liveness GetLocalProcessWindow(const gfx::Point& screen_point,
                                               bool exclude_dragged_view,
                                               gfx::NativeWindow* window);

  // Tests whether a drag can be attached to a `window`.  Drags may be
  // disallowed for reasons such as the target: does not support tabs, is
  // showing a modal, has a different profile, is a different browser type
  // (NORMAL vs APP).
  bool CanAttachTo(gfx::NativeWindow window);

  // Helper method for OnSystemDnDExited() to calculate a y-coordinate that is
  // out of the bounds of `attached_context_`, keeping
  // `kVerticalDetachMagnetism` in mind.
  int GetOutOfBoundsYCoordinate() const;

  // Helper method to ElementTracker events when a tab has been added to a group
  // as a result of a drag finishing.
  void NotifyEventIfTabAddedToGroup();

  // Similar implementations present in
  // chrome/browser/ui/webui/tab_strip/tab_strip_page_handler.cc. If logic  is
  // updated in one, the other should also be updated.
  void MaybePauseTrackingSavedTabGroup();
  void MaybeResumeTrackingSavedTabGroup();

  // Initializes `dragging_tabs_session_`, and performs a first MoveAttached
  // within `attached_context_`.
  void StartDraggingTabsSession(bool initial_move,
                                gfx::Point start_point_in_screen);

  // Calls `SetSelectionFromModel` in the `tab_strip_model`. This centralizes
  // the logic for retaining previous active and anchor tabs for split.
  void UpdateSelectionModel(TabStripModel* tab_strip_model,
                            ui::ListSelectionModel selection_model);

#if defined(USE_AURA)
  // aura::client::DragDropClientObserver:
  void OnDragStarted() override;
  void OnDragDropClientDestroying() override;
#endif  // defined(USE_AURA)

  // Updates the current drag target, and fires relevant handler events.
  void UpdateDragTarget(TabDragDelegate* new_target);
  void ResetDragTarget();

  static void SetTabDragPointResolver(TabDragPointResolver& resolver);

  DragState current_state_ = DragState::kNotStarted;

  std::unique_ptr<DraggingTabsSession> dragging_tabs_session_;

  ui::mojom::DragEventSource event_source_ = ui::mojom::DragEventSource::kMouse;

  // The TabDragContext the drag originated from. This is set to null
  // if destroyed during the drag.
  raw_ptr<TabDragContext> source_context_;

  // The TabDragContext the dragged Tab is currently attached to, or
  // null if the dragged Tab is detached.
  raw_ptr<TabDragContext, DanglingUntriaged> attached_context_;

  // Whether capture can be released during the drag. When false, capture should
  // not be released when transferring capture between widgets and when starting
  // the move loop.
  bool can_release_capture_;

  // The position of the mouse (in screen coordinates) at the start of the drag
  // operation. This is used to calculate minimum elasticity before a
  // DraggedTabView is constructed.
  gfx::Point start_point_in_screen_;

  // Ratio of the x-coordinate of the `source_view_offset` to the width of the
  // source view.
  float offset_to_width_ratio_;

  // Used to track the view that had focus in the window containing
  // `source_view_`. This is saved so that focus can be restored properly when
  // a drag begins and ends within this same window.
  std::unique_ptr<views::ViewTracker> old_focused_view_tracker_;

  // Timer used to bring the window under the cursor to front. If the user
  // stops moving the mouse for a brief time over a browser window, it is
  // brought to front.
  base::OneShotTimer bring_to_front_timer_;

  DragSessionData drag_data_;

  // Used to pause observation of all open SavedTabGroups when a drag is
  // occurring. This object is assigned when MaybePauseTrackingSavedTabGroup()
  // is called and reset when MaybeResumeTrackingSavedTabGroup() is called. The
  // primary use cases are when we are in the middle of a drag session (Paired
  // Detach() and Attach() calls) or when reverting a drag (RevertDrag()).
  std::unique_ptr<tab_groups::ScopedLocalObservationPauser> observation_pauser_;

  // The selection model before the drag started. See comment above Init() for
  // details.
  ui::ListSelectionModel initial_selection_model_;

  // The selection model of `attached_context_` before the tabs were attached.
  ui::ListSelectionModel selection_model_before_attach_;

  // What should occur during ContinueDragging when a tab is attempted to be
  // detached.
  DetachBehavior detach_behavior_;

  // Last location used in screen coordinates.
  gfx::Point last_point_in_screen_ = gfx::Point();

  // The following are needed when detaching into a browser
  // (`detach_into_browser_` is true).

  // True if `attached_context_` is in a browser specifically created for
  // the drag.
  bool is_dragging_new_browser_;

  // True if `source_context_` was maximized before the drag.
  bool was_source_maximized_;

  // True if `source_context_` was in immersive fullscreen before the drag.
  bool was_source_fullscreen_;

  // True if the initial drag resulted in restoring the window (because it was
  // maximized).
  bool did_restore_window_;

  // The TabDragContext to attach to after the move loop completes.
  raw_ptr<TabDragContext> tab_strip_to_attach_to_after_exit_;

  // Non-null for the duration of RunMoveLoop.
  raw_ptr<views::Widget> move_loop_widget_;

  // See description above getter.
  bool is_mutating_;

  // Called when the loop in RunMoveLoop finishes / system DnD session ends.
  // Only for tests.
  base::OnceClosure drag_loop_done_callback_;

  // Used in a system-DnD-based drag session if we need to hide a window with
  // all of its tabs being dragged, as that needs to happen after starting the
  // DnD session.
  base::OnceClosure drag_started_callback_;

  // The tab thumbnail used as the drag icon for system-DnD-based tab dragging.
  gfx::ImageSkia drag_image_;

#if defined(USE_AURA)
  base::ScopedObservation<aura::client::DragDropClient,
                          aura::client::DragDropClientObserver>
      drag_drop_client_observation_{this};
#endif  // defined(USE_AURA)

  std::unique_ptr<EventTracker> event_tracker_;

  std::unique_ptr<SourceTabStripEmptinessTracker>
      source_context_emptiness_tracker_;

  std::unique_ptr<DraggedTabsClosedTracker>
      attached_context_tabs_closed_tracker_;

  std::unique_ptr<WindowFinder> window_finder_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // True while RunMoveLoop() has been called on a widget.
  bool in_move_loop_ = false;

  // Used by StartSystemDnDSessionIfNecessary() and IsSystemDnDSessionRunning().
  // This cannot be deduced from `current_state_`, because the system drag
  // session keeps running even when `current_state_` changes back to
  // `kDraggingTabs`, and we must not start a new system drag session the next
  // time StartSystemDnDSessionIfNecessary() is called.
  bool system_drag_and_drop_session_running_ = false;

  // True if in the process of moving the last tab in
  // the TabStrip out of `attached_context_` so that it can be
  // inserted into another TabStrip.
  bool is_moving_last_tab_ = false;

  // Use a base::WeakAutoReset to set this to true when calling methods that
  // theoretically could lead to ending the drag, but should not. For example,
  // methods that synthesize events, take/release capture, etc., where whether
  // or not they destroy `this` might depend on platform behavior or other
  // external factors. Destruction while this is true will DCHECK.
  bool expect_stay_alive_ = false;

  // The current candidate that may handle a tab drop.
  raw_ptr<TabDragDelegate> current_drag_delegate_;
  base::CallbackListSubscription drag_delegate_destroyed_subscription_;

  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  base::WeakPtrFactory<TabDragController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_H_
