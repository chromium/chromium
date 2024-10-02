// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tabs/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_strip_scroll_session.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#endif  // defined(USE_AURA)

namespace ui {
class ListSelectionModel;
class PresentationTimeRecorder;
}
namespace views {
class View;
class ViewTracker;
}
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
class WindowFinder;
class TabStripScrollSession;

class TabDragWithScrollManager {
 public:
  virtual ~TabDragWithScrollManager() = default;
  // Check if the current DragState is `kDraggingTabs`.
  virtual bool IsDraggingTabState() = 0;
  // Handles dragging tabs while the tabs are attached. `just_attached` should
  // be true iff this is the first call to MoveAttached after attaching. This
  // also starts a scroll session if needed.
  // TODO(crbug.com/40875136): Make this an observer of the scroll_session
  // class.
  virtual void MoveAttached(const gfx::Point& point_in_screen,
                            bool just_attached) = 0;
  // Returns a rect starting from the origin of the first dragged tab
  // to the end of the last dragged tab.
  virtual gfx::Rect GetEnclosingRectForDraggedTabs() = 0;
  // Returns the point in screen when the last drag occurred.
  virtual gfx::Point GetLastPointInScreen() = 0;
  // Returns the `attached_context_`.
  virtual views::View* GetAttachedContext() = 0;
  // Get Scroll View from the `attached_context_`.
  virtual views::ScrollView* GetScrollView() = 0;
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
                          public TabDragWithScrollManager
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
  // drag. In particular this will require reconciliation with
  // `DragBrowserResultType` returned by `DragBrowserToNewTabStrip`. Currently
  // the following public methods may end the drag and destroy `this` but do not
  // return a Liveness:
  // - TabWasAdded
  // - OnTabWillBeRemoved
  // - Drag
  // - EndDrag (this always end the drag)
  //
  // The static methods OnSystemDragAndDropUpdated and OnSystemDragAndDropExited
  // may also end the drag.
  //
  // There are also many private methods that may end the drag but don't return
  // a Liveness, and there is at least one case where the wrong Liveness might
  // be returned due to the interaction with DragBrowserToNewTabStrip mentioned
  // above.
  enum class Liveness {
    ALIVE,
    DELETED,
  };

  // Initializes TabDragController to drag the views in |dragging_views|
  // originating from |source_context|. |source_view| is the view that
  // initiated the drag and is either a Tab or a TabGroupHeader contained in
  // |dragging_views|. |mouse_offset| is the distance of the mouse pointer from
  // the origin of the first view in |dragging_views| and |source_view_offset|
  // the offset from |source_view|. |source_view_offset| is the horizontal
  // offset of |mouse_offset| relative to |source_view|.
  // |initial_selection_model| is the selection model before the drag started
  // and is only non-empty if the original selection isn't the same as the
  // dragging set. Returns Liveness::DELETED if `this` was deleted during this
  // call, and Liveness::ALIVE if `this` still exists.
  [[nodiscard]] Liveness Init(
      TabDragContext* source_context,
      TabSlotView* source_view,
      const std::vector<raw_ptr<TabSlotView, VectorExperimental>>&
          dragging_views,
      const gfx::Point& mouse_offset,
      int source_view_offset,
      ui::ListSelectionModel initial_selection_model,
      ui::mojom::DragEventSource event_source);

  // Returns true if there is a drag underway and the drag is attached to
  // |tab_strip|.
  // NOTE: this returns false if the TabDragController is in the process of
  // finishing the drag.
  static bool IsAttachedTo(const TabDragContextBase* tab_strip);

  // Returns true if there is a drag underway.
  static bool IsActive();

  // Returns true if a regular drag and drop session is running as a fallback
  // instead of a move loop.
  static bool IsSystemDragAndDropSessionRunning();

  // Called by TabStrip / TabStripRegionView to inform TabDragController.
  static void OnSystemDragAndDropUpdated(const ui::DropTargetEvent& event);
  static void OnSystemDragAndDropExited();
  static void OnSystemDragAndDropEnded();

  // Returns the pointer of |source_context_|.
  static TabDragContext* GetSourceContext();

  enum class ScrollWithDragStrategy {
    kConstantSpeed = 1,
    kVariableSpeed = 2,
    kDisabled = 3
  };

  ui::mojom::DragEventSource event_source() const { return event_source_; }

  // See description above fields for details on these.
  bool active() const { return current_state_ != DragState::kStopped; }
  const TabDragContext* attached_context() const { return attached_context_; }

  // Returns true if a drag started.
  bool started_drag() const { return current_state_ != DragState::kNotStarted; }

  // Returns the tab group being dragged, if any. Will only return a value if
  // the user is dragging a tab group header, not an individual tab or tabs from
  // a group.
  const std::optional<tab_groups::TabGroupId>& group() const { return group_; }

  bool IsRemovingLastTabForRevert() const {
    return is_removing_last_tab_for_revert_;
  }

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
  void Drag(const gfx::Point& point_in_screen);

  // Complete the current drag session.
  void EndDrag(EndDragReason reason);

  // Set a callback to be called when the nested drag loop / system DnD session
  // finishes. Note that the latter only ends when the mouse is released, i.e.
  // the callback won't be invoked when attaching to another browser if the tab
  // dragging session continues running.
  void SetDragLoopDoneCallbackForTesting(base::OnceClosure callback);

  TabStripScrollSession* GetTabStripScrollSessionForTesting() {
    return tab_strip_scroll_session_.get();
  }

 private:
  friend class TabDragControllerTest;

  class SourceTabStripEmptinessTracker;

  class DraggedTabsClosedTracker;

  // Used to indicate the direction the mouse has moved when attached.
  static const int kMovedMouseLeft  = 1 << 0;
  static const int kMovedMouseRight = 1 << 1;

  enum class DragState {
    // The drag has not yet started; the user has not dragged far enough to
    // begin a session.
    kNotStarted,
    // The session is dragging a set of tabs within |attached_context_|.
    kDraggingTabs,
    // The session is dragging a window; |attached_context_| is that window's
    // tabstrip.
    kDraggingWindow,
    // The platform does not support client controlled window dragging; instead,
    // a regular drag and drop session is running. The dragged tabs are still
    // moved to a new browser, but it stays hidden until the drag ends. On
    // platforms where this state is used, the kDraggingWindow and
    // kWaitingToDragTabs states are not used.
    kDraggingUsingSystemDragAndDrop,
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
  enum EndDragType {
    // Drag session exited normally: the user released the mouse.
    NORMAL,

    // The drag session was canceled (alt-tab during drag, escape ...)
    CANCELED,

    // The tab (NavigationController) was destroyed during the drag.
    TAB_DESTROYED
  };

  // Whether Detach() should release capture or not.
  enum ReleaseCapture {
    RELEASE_CAPTURE,
    DONT_RELEASE_CAPTURE,
  };

  // Enumeration of the possible positions the detached tab may detach from.
  enum DetachPosition {
    DETACH_BEFORE,
    DETACH_AFTER,
    DETACH_ABOVE_OR_BELOW
  };

  // Specifies what should happen when a drag motion exits the tab strip region
  // in an attempt to detach a tab.
  enum DetachBehavior {
    DETACHABLE,
    NOT_DETACHABLE
  };

  // Indicates what should happen after invoking DragBrowserToNewTabStrip().
  enum DragBrowserResultType {
    // The caller should return immediately. This return value is used if a
    // nested run loop was created or we're in a nested run loop and
    // need to exit it.
    DRAG_BROWSER_RESULT_STOP,

    // The caller should continue.
    DRAG_BROWSER_RESULT_CONTINUE,
  };

  // Stores the date associated with a single tab that is being dragged.
  struct TabDragData {
    TabDragData();
    TabDragData(const TabDragData&) = delete;
    TabDragData& operator=(const TabDragData&) = delete;
    ~TabDragData();
    TabDragData(TabDragData&&);

    // The WebContents being dragged.
    raw_ptr<content::WebContents> contents;

    // There is a brief period of time when a tab is being moved from one tab
    // strip to another [after Detach but before Attach] that the TabDragData
    // owns the WebContents.
    std::unique_ptr<tabs::TabModel> owned_tab;

    // This is the index of the tab in |source_context_| when the drag
    // began. This is used to restore the previous state if the drag is aborted.
    // Nullopt if this is a group header.
    std::optional<int> source_model_index;

    // If attached this is the view in |attached_context_|.
    raw_ptr<TabSlotView, DanglingUntriaged> attached_view;

    // Is the tab pinned?
    bool pinned;

    // Contains the information for the tab's group at the start of the drag.
    struct TabGroupData {
      tab_groups::TabGroupId group_id;
      tab_groups::TabGroupVisualData group_visual_data;
    };

    // Stores the information of the group the tab is in, or nullopt if tab is
    // not grouped.
    std::optional<TabGroupData> tab_group_data;
  };

  typedef std::vector<TabDragData> DragData;

  // Sets |drag_data| from |view|. This also registers for necessary
  // notifications and resets the delegate of the WebContents.
  void InitDragData(TabSlotView* view, TabDragData* drag_data);

  // Overridden from views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  // Forget the source tabstrip. It doesn't exist any more, so it doesn't
  // make sense to insert dragged tabs back into it if the drag is reverted.
  void OnSourceTabStripEmpty();

  // A tab was closed in the active tabstrip. Clean up if we were dragging it.
  void OnActiveStripWebContentsRemoved(content::WebContents* contents);

  // The WebContents in a tab was replaced in the active tabstrip. Update our
  // canonical reference if we were dragging that tab.
  void OnActiveStripWebContentsReplaced(content::WebContents* previous,
                                        content::WebContents* next);

  // Initialize the offset used to calculate the position to create windows
  // in |GetWindowCreatePoint|. This should only be invoked from |Init|.
  void InitWindowCreatePoint();

  // TabDragWithScrollManager:
  gfx::Point GetLastPointInScreen() override;
  views::View* GetAttachedContext() override;
  views::ScrollView* GetScrollView() override;
  bool IsDraggingTabState() override;
  gfx::Rect GetEnclosingRectForDraggedTabs() override;
  void MoveAttached(const gfx::Point& point_in_screen,
                    bool just_attached) override;

  // Returns the point where a detached window should be created given the
  // current mouse position |origin|.
  gfx::Point GetWindowCreatePoint(const gfx::Point& origin) const;

  void UpdateDockInfo(const gfx::Point& point_in_screen);

  // Saves focus in the window that the drag initiated from. Focus will be
  // restored appropriately if the drag ends within this same window.
  void SaveFocus();

  // Restore focus to the View that had focus before the drag was started, if
  // the drag ends within the same Window as it began.
  void RestoreFocus();

  // Tests whether |point_in_screen| is past a minimum elasticity threshold
  // required to start a drag.
  bool CanStartDrag(const gfx::Point& point_in_screen) const;

  // Invoked once a drag has started to determine the appropriate context to
  // drag to (which may be the currently attached one).
  [[nodiscard]] Liveness ContinueDragging(const gfx::Point& point_in_screen);

  // Transitions dragging from |attached_context_| to |target_context|.
  // |target_context| is NULL if the mouse is not over a valid tab strip.  See
  // DragBrowserResultType for details of the return type.
  DragBrowserResultType DragBrowserToNewTabStrip(
      TabDragContext* target_context,
      const gfx::Point& point_in_screen);

  // Returns true if we should use a system drag and drop session when dragging
  // tabs outside of a tabstrip.
  bool ShouldDragWindowUsingSystemDnD();

  // Returns the drag image to be used for the system drag and drop session.
  gfx::ImageSkia GetDragImageForSystemDnD();

  // Starts a regular drag and drop session as a fallback if RunMoveLoop() is
  // not supported and no drag session is currently running. `context` is used
  // to get the widget that will initiate the drag session, and can be NULL if
  // `system_drag_and_drop_session_running_` is true.
  Liveness StartSystemDnDSessionIfNecessary(TabDragContext* context,
                                            const gfx::Point& point_in_screen);

  // Stored as a callback in `drag_started_callback_`. See the comment in
  // StartSystemDnDSessionIfNecessary() for more details.
  void HideAttachedContext();

  // Returns the compatible TabDragContext to drag to at the
  // specified point (screen coordinates), or nullptr if there is none.
  Liveness GetTargetTabStripForPoint(const gfx::Point& point_in_screen,
                                     TabDragContext** tab_strip);

  // Returns true if |context| contains the specified point in screen
  // coordinates.
  bool DoesTabStripContain(TabDragContext* context,
                           const gfx::Point& point_in_screen) const;

  // Returns the DetachPosition given the specified location in screen
  // coordinates.
  DetachPosition GetDetachPosition(const gfx::Point& point_in_screen);

  // Attach the dragged tabs to the specified TabDragContext. If
  // |set_capture| is true, the newly attached context will have capture.
  // This can only be used to pass ownership of |this| through |controller|.
  // |controller| must be nullptr if |attached_context| already owns |this|.
  void Attach(TabDragContext* attached_context,
              const gfx::Point& point_in_screen,
              std::unique_ptr<TabDragController> controller,
              bool set_capture = true);

  // Detach the dragged tabs from the current TabDragContext. Returns
  // ownership of the owned controller, which must be |this|, if
  // |attached_context_| currently owns a controller. Otherwise returns
  // nullptr.
  std::unique_ptr<TabDragController> Detach(ReleaseCapture release_capture);

  // Detach from |attached_context_| and attach to |target_context| instead.
  // See Detach/Attach for parameter documentation. Transfers ownership of
  // |this| from |attached_context_| (which must own |this|) to
  // |target_context|.
  void DetachAndAttachToNewContext(ReleaseCapture release_capture,
                                   TabDragContext* target_context,
                                   const gfx::Point& point_in_screen,
                                   bool set_capture = true);

  // Detaches the tabs being dragged, creates a new Browser to contain them and
  // runs a nested move loop.
  void DetachIntoNewBrowserAndRunMoveLoop(const gfx::Point& point_in_screen);

  // Runs a nested run loop that handles moving the current
  // Browser. |drag_offset| is the offset from the window origin and is used in
  // calculating the location of the window offset from the cursor while
  // dragging.
  void RunMoveLoop(const gfx::Vector2d& drag_offset);

  // Retrieves the bounds of the dragged tabs relative to the attached
  // TabDragContext. |tab_strip_point| is in the attached
  // TabDragContext's coordinate system.
  gfx::Rect GetDraggedViewTabStripBounds(const gfx::Point& tab_strip_point);

  // Gets the position of the dragged tabs relative to the attached tab strip
  // with the mirroring transform applied.
  gfx::Point GetAttachedDragPoint(const gfx::Point& point_in_screen);

  // Finds the TabSlotViews within the specified TabDragContext that
  // corresponds to the WebContents of the dragged views. Also finds the group
  // header if it is dragging. Returns an empty vector if not attached.
  std::vector<raw_ptr<TabSlotView, VectorExperimental>>
  GetViewsMatchingDraggedContents(TabDragContext* context);

  // Does the work for EndDrag(). If we actually started a drag and |how_end| is
  // not TAB_DESTROYED then one of CompleteDrag() or RevertDrag() is invoked.
  void EndDragImpl(EndDragType how_end);

  // Reverts a cancelled drag operation.
  void RevertDrag();

  // Reverts the drag for all the tabs belonging to a group.
  void RevertHeaderDrag(tab_groups::TabGroupId group_id);

  // Reverts the tab at |drag_index| in |drag_data_|.
  void RevertDragAt(size_t drag_index);

  // Selects the dragged tabs in |model|. Does nothing if there are no longer
  // any dragged contents (as happens when a WebContents is deleted out from
  // under us).
  void ResetSelection(TabStripModel* model);

  // Restores |initial_selection_model_| to the |source_context_|.
  void RestoreInitialSelection();

  // Finishes a successful drag operation.
  void CompleteDrag();

  // Maximizes the attached window.
  void MaximizeAttachedWindow();

  // Hides the frame for the window that contains the TabDragContext
  // the current drag session was initiated from.
  void HideFrame();

  void BringWindowUnderPointToFront(const gfx::Point& point_in_screen);

  // Convenience for getting the TabDragData corresponding to the source view
  // that the user started dragging.
  TabDragData* source_view_drag_data() {
    return &(drag_data_[source_view_index_]);
  }

  // Convenience for |source_view_drag_data()->contents|.
  content::WebContents* source_dragged_contents() {
    return source_view_drag_data()->contents;
  }

  // Returns the number of Tab views currently dragging.
  // Excludes the TabGroupHeader view, if any.
  int num_dragging_tabs() {
    return header_drag_ ? drag_data_.size() - 1 : drag_data_.size();
  }

  // Returns true if currently dragging a tab with |contents|.
  bool IsDraggingTab(content::WebContents* contents) const;

  // Returns the index of the first Tab, since the first dragging view may
  // instead be a TabGroupHeader.
  int first_tab_index() { return header_drag_ ? 1 : 0; }

  // Returns the Widget of the currently attached TabDragContext's
  // BrowserView.
  views::Widget* GetAttachedBrowserWidget();

  // Returns true if the tabs were originally one after the other in
  // |source_context_|.
  bool AreTabsConsecutive();

  // Calculates and returns new bounds for the dragged browser window.
  // Takes into consideration current and restore bounds of |source| tab strip
  // preventing the dragged size from being too small. Positions the new bounds
  // such that the tab that was dragged remains under the |point_in_screen|.
  // Offsets |drag_bounds| if necessary when dragging to the right from the
  // source browser.
  gfx::Rect CalculateDraggedBrowserBounds(TabDragContext* source,
                                          const gfx::Point& point_in_screen,
                                          std::vector<gfx::Rect>* drag_bounds);

  // Calculates and returns the dragged bounds for the non-maximize dragged
  // browser window. Takes into consideration the initial drag offset so that
  // the dragged tab remains under the |point_in_screen|.
  gfx::Rect CalculateNonMaximizedDraggedBrowserBounds(
      views::Widget* widget,
      const gfx::Point& point_in_screen);

  // Calculates scaled `drag_bounds` for dragged tabs and sets the tabs bounds.
  // Layout of the tabstrip is performed and a new tabstrip width calculated.
  // When `previous_tab_area_width` is larger than the new tab area width the
  // tabs in the attached tabstrip are scaled and repositioned and the attached
  // browser is positioned such that the tab that was dragged remains under the
  // `point_in_screen`. `drag_offset` is the offset of `point_in_screen` from
  // the origin of the dragging browser window, and will be updated when this
  // method ends up with changing the origin of the attached browser window.
  void AdjustBrowserAndTabBoundsForDrag(int previous_tab_area_width,
                                        const gfx::Point& point_in_screen,
                                        gfx::Vector2d* drag_offset,
                                        std::vector<gfx::Rect>* drag_bounds);

  // If the user is dragging a single tab that is controlled by one web app,
  // and features::kTearOffWebAppTabOpensWebAppWindow is enabled,
  // returns the app id of that web app, nullopt otherwise.
  std::optional<webapps::AppId> GetControllingAppForDrag(Browser* browser);

  // Creates and returns a new Browser to handle the drag.
  Browser* CreateBrowserForDrag(TabDragContext* source,
                                const gfx::Point& point_in_screen,
                                gfx::Vector2d* drag_offset,
                                std::vector<gfx::Rect>* drag_bounds);

  // Returns the location of the cursor. This is either the location of the
  // mouse or the location of the current touch point.
  gfx::Point GetCursorScreenPoint();

  // Returns the offset from the top left corner of the window to
  // |point_in_screen|.
  gfx::Vector2d GetWindowOffset(const gfx::Point& point_in_screen);

  // Returns the NativeWindow in |window| at the specified point. If
  // |exclude_dragged_view| is true, then the dragged view is not considered.
  [[nodiscard]] Liveness GetLocalProcessWindow(const gfx::Point& screen_point,
                                               bool exclude_dragged_view,
                                               gfx::NativeWindow* window);

  // Sets the dragging info for the current dragged context. On Chrome OS, the
  // dragging info include two window properties: one is to indicate if the
  // tab-dragging process starts/stops, and the other is to indicate which
  // window initiates the dragging. This function is supposed to be called
  // whenever the dragged tabs are attached to a new tabstrip.
  void SetTabDraggingInfo();

  // Clears the tab dragging info for the current dragged context. This
  // function is supposed to be called whenever the dragged tabs are detached
  // from the old context or the tab dragging is ended.
  void ClearTabDraggingInfo();

  // Tests whether a drag can be attached to a |window|.  Drags may be
  // disallowed for reasons such as the target: does not support tabs, is
  // showing a modal, has a different profile, is a different browser type
  // (NORMAL vs APP).
  bool CanAttachTo(gfx::NativeWindow window);

  // Helper method for TabDragController::MoveAttached to precompute the tab
  // group membership of selected tabs after performing the move.
  std::optional<tab_groups::TabGroupId> CalculateGroupForDraggedTabs(
      int to_index);

  // Helper method for OnSystemDragAndDropExited() to calculate a y-coordinate
  // that is out of the bounds of |attached_context_|, keeping
  // |kVerticalDetachMagnetism| in mind.
  int GetOutOfBoundsYCoordinate() const;

  // Helper method to ElementTracker events when a tab has been added to a group
  // as a result of a drag finishing.
  void NotifyEventIfTabAddedToGroup();

  void MaybePauseTrackingSavedTabGroup();

  void MaybeResumeTrackingSavedTabGroup();

#if defined(USE_AURA)
  // aura::client::DragDropClientObserver:
  void OnDragStarted() override;
  void OnDragDropClientDestroying() override;
#endif  // defined(USE_AURA)

  DragState current_state_;

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

  // This is the offset of the mouse from the top left of the first Tab where
  // dragging began. This is used to ensure that the dragged view is always
  // positioned at the correct location during the drag, and to ensure that the
  // detached window is created at the right location.
  gfx::Point mouse_offset_;

  // Ratio of the x-coordinate of the |source_view_offset| to the width of the
  // source view.
  float offset_to_width_ratio_;

  // A hint to use when positioning new windows created by detaching Tabs. This
  // is the distance of the mouse from the top left of the dragged tab as if it
  // were the distance of the mouse from the top left of the first tab in the
  // attached TabDragContext from the top left of the window.
  gfx::Point window_create_point_;

  // Location of the first tab in the source tabstrip in screen coordinates.
  // This is used to calculate |window_create_point_|.
  gfx::Point first_source_tab_point_;

  // Used to track the view that had focus in the window containing
  // |source_view_|. This is saved so that focus can be restored properly when
  // a drag begins and ends within this same window.
  std::unique_ptr<views::ViewTracker> old_focused_view_tracker_;

  // The horizontal position of the mouse cursor in `attached_context_`
  // coordinates at the time of the last re-order event.
  int last_move_attached_context_loc_ = 0;

  // Timer used to bring the window under the cursor to front. If the user
  // stops moving the mouse for a brief time over a browser window, it is
  // brought to front.
  base::OneShotTimer bring_to_front_timer_;

  DragData drag_data_;

  // Index of the source view in |drag_data_|.
  size_t source_view_index_;

  // The attached views. Also found in |drag_data_|, but cached for convenience.
  std::vector<raw_ptr<TabSlotView, VectorExperimental>> attached_views_;

  // Whether the drag originated from a group header.
  bool header_drag_;

  // The group that is being dragged. Only set if the drag originated from a
  // group header, indicating that the entire group is being dragged together.
  std::optional<tab_groups::TabGroupId> group_;

  // Used to pause observation of all open SavedTabGroups when a drag is
  // occurring. This object is assigned when MaybePauseTrackingSavedTabGroup()
  // is called and reset when MaybeResumeTrackingSavedTabGroup() is called. The
  // primary use cases are when we are in the middle of a drag session (Paired
  // Detach() and Attach() calls) or when reverting a drag (RevertDrag()).
  std::unique_ptr<tab_groups::ScopedLocalObservationPauser> observation_pauser_;

  // True until MoveAttached() is first invoked.
  bool initial_move_;

  // The selection model before the drag started. See comment above Init() for
  // details.
  ui::ListSelectionModel initial_selection_model_;

  // The selection model of |attached_context_| before the tabs were attached.
  ui::ListSelectionModel selection_model_before_attach_;

  // What should occur during ContinueDragging when a tab is attempted to be
  // detached.
  DetachBehavior detach_behavior_;

  // Last location used in screen coordinates.
  gfx::Point last_point_in_screen_ = gfx::Point();

  // The following are needed when detaching into a browser
  // (|detach_into_browser_| is true).

  // True if |attached_context_| is in a browser specifically created for
  // the drag.
  bool is_dragging_new_browser_;

  // True if |source_context_| was maximized before the drag.
  bool was_source_maximized_;

  // True if |source_context_| was in immersive fullscreen before the drag.
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

  // Used by StartSystemDragAndDropSessionIfNecessary() and
  // IsSystemDragAndDropSessionRunning().
  // This cannot be deduced from |current_state_|, because the system drag
  // session keeps running even when |current_state_| changes back to
  // |kDraggingTabs|, and we must not start a new system drag session the next
  // time StartSystemDragAndDropSessionIfNecessary() is called.
  bool system_drag_and_drop_session_running_ = false;

  // Returns true if in the process of reverting the drag and the last tab in
  // the TabStrip is being removed from `attached_context_` so that it can be
  // inserted back into `source_context_`.
  bool is_removing_last_tab_for_revert_ = false;

  // Use a base::WeakAutoReset to set this to true when calling methods that
  // theoretically could lead to ending the drag, but should not. For example,
  // methods that synthesize events, take/release capture, etc., where whether
  // or not they destroy `this` might depend on platform behavior or other
  // external factors. Destruction while this is true will DCHECK.
  bool expect_stay_alive_ = false;
  ScrollWithDragStrategy drag_with_scroll_mode_ =
      ScrollWithDragStrategy::kDisabled;

  // the scrolling session that handles scrolling when the tabs are dragged
  // to the scrollable regions of the tab_strip.
  std::unique_ptr<TabStripScrollSession> tab_strip_scroll_session_ = nullptr;

  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  base::WeakPtrFactory<TabDragController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
