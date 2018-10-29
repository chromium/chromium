// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget_observer.h"

#if defined(USE_AURA)
namespace aura {
class Env;
}
#endif

namespace ui {
class ListSelectionModel;
}
namespace views {
class View;
class ViewTracker;
}
class Browser;
class EscapeTracker;
class Tab;
class TabDragControllerTest;
class TabStrip;
class TabStripModel;
class WindowFinder;

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
                          public TabStripModelObserver {
 public:
  // What should happen as the mouse is dragged within the tabstrip.
  enum MoveBehavior {
    // Only the set of visible tabs should change. This is only applicable when
    // using touch layout.
    MOVE_VISIBLE_TABS,

    // Typical behavior where tabs are dragged around.
    REORDER
  };

  // Indicates the event source that initiated the drag.
  enum EventSource {
    EVENT_SOURCE_MOUSE,
    EVENT_SOURCE_TOUCH,
  };

  // Amount above or below the tabstrip the user has to drag before detaching.
  static const int kTouchVerticalDetachMagnetism;
  static const int kVerticalDetachMagnetism;

  TabDragController();
  ~TabDragController() override;

  // Initializes TabDragController to drag the tabs in |tabs| originating from
  // |source_tabstrip|. |source_tab| is the tab that initiated the drag and is
  // contained in |tabs|.  |mouse_offset| is the distance of the mouse pointer
  // from the origin of the first tab in |tabs| and |source_tab_offset| the
  // offset from |source_tab|. |source_tab_offset| is the horizontal offset of
  // |mouse_offset| relative to |source_tab|. |initial_selection_model| is the
  // selection model before the drag started and is only non-empty if
  // |source_tab| was not initially selected.
  void Init(TabStrip* source_tabstrip,
            Tab* source_tab,
            const std::vector<Tab*>& tabs,
            const gfx::Point& mouse_offset,
            int source_tab_offset,
            ui::ListSelectionModel initial_selection_model,
            MoveBehavior move_behavior,
            EventSource event_source);

  // Returns true if there is a drag underway and the drag is attached to
  // |tab_strip|.
  // NOTE: this returns false if the TabDragController is in the process of
  // finishing the drag.
  static bool IsAttachedTo(const TabStrip* tab_strip);

  // Returns true if there is a drag underway.
  static bool IsActive();

  // Sets the move behavior. Has no effect if started_drag() is true.
  void SetMoveBehavior(MoveBehavior behavior);
  MoveBehavior move_behavior() const { return move_behavior_; }

  EventSource event_source() const { return event_source_; }

  // See description above fields for details on these.
  bool active() const { return active_; }
  const TabStrip* attached_tabstrip() const { return attached_tabstrip_; }

  // Returns true if a drag started.
  bool started_drag() const { return started_drag_; }

  // Returns true if mutating the TabStripModel.
  bool is_mutating() const { return is_mutating_; }

  // Returns true if we've detached from a tabstrip and are running a nested
  // move message loop.
  bool is_dragging_window() const { return is_dragging_window_; }

  // Returns true if currently dragging a tab with |contents|.
  bool IsDraggingTab(content::WebContents* contents);

  // Invoked to drag to the new location, in screen coordinates.
  void Drag(const gfx::Point& point_in_screen);

  // Complete the current drag session.
  void EndDrag(EndDragReason reason);

 private:
  friend class TabDragControllerTest;

  // Used to indicate the direction the mouse has moved when attached.
  static const int kMovedMouseLeft  = 1 << 0;
  static const int kMovedMouseRight = 1 << 1;

  enum class Liveness {
    ALIVE,
    DELETED,
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

  // Specifies what should happen when RunMoveLoop completes.
  enum EndRunLoopBehavior {
    // Indicates the drag should end.
    END_RUN_LOOP_STOP_DRAGGING,

    // Indicates the drag should continue.
    END_RUN_LOOP_CONTINUE_DRAGGING
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
    ~TabDragData();
    TabDragData(TabDragData&&);

    // The WebContents being dragged.
    content::WebContents* contents;

    // There is a brief period of time when a tab is being moved from one tab
    // strip to another [after Detach but before Attach] that the TabDragData
    // owns the WebContents.
    std::unique_ptr<content::WebContents> owned_contents;

    // This is the index of the tab in |source_tabstrip_| when the drag
    // began. This is used to restore the previous state if the drag is aborted.
    int source_model_index;

    // If attached this is the tab in |attached_tabstrip_|.
    Tab* attached_tab;

    // Is the tab pinned?
    bool pinned;

   private:
    DISALLOW_COPY_AND_ASSIGN(TabDragData);
  };

  typedef std::vector<TabDragData> DragData;

#if defined(OS_CHROMEOS)
  class DeferredTargetTabstripObserver;
#endif

  // Sets |drag_data| from |tab|. This also registers for necessary
  // notifications and resets the delegate of the WebContents.
  void InitTabDragData(Tab* tab, TabDragData* drag_data);

  // Overriden from views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // Overriden from TabStripModelObserver:
  void TabStripEmpty() override;

  // Initialize the offset used to calculate the position to create windows
  // in |GetWindowCreatePoint|. This should only be invoked from |Init|.
  void InitWindowCreatePoint();

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

  // Invoked once a drag has started to determine the appropriate tabstrip to
  // drag to (which may be the currently attached one).
  Liveness ContinueDragging(const gfx::Point& point_in_screen)
      WARN_UNUSED_RESULT;

  // Transitions dragging from |attached_tabstrip_| to |target_tabstrip|.
  // |target_tabstrip| is NULL if the mouse is not over a valid tab strip.  See
  // DragBrowserResultType for details of the return type.
  DragBrowserResultType DragBrowserToNewTabStrip(
      TabStrip* target_tabstrip,
      const gfx::Point& point_in_screen);

  // Handles dragging for a touch tabstrip when the tabs are stacked. Doesn't
  // actually reorder the tabs in anyway, just changes what's visible.
  void DragActiveTabStacked(const gfx::Point& point_in_screen);

  // Moves the active tab to the next/previous tab. Used when the next/previous
  // tab is stacked.
  void MoveAttachedToNextStackedIndex(const gfx::Point& point_in_screen);
  void MoveAttachedToPreviousStackedIndex(const gfx::Point& point_in_screen);

  // Handles dragging tabs while the tabs are attached.
  void MoveAttached(const gfx::Point& point_in_screen);

  // If necessary starts the |move_stacked_timer_|. The timer is started if
  // close enough to an edge with stacked tabs.
  void StartMoveStackedTimerIfNecessary(
      const gfx::Point& point_in_screen,
      int delay_ms);

  // Returns the TabStrip for the specified window, or NULL if one doesn't exist
  // or isn't compatible.
  TabStrip* GetTabStripForWindow(gfx::NativeWindow window);

  // Returns the compatible TabStrip to drag to at the specified point (screen
  // coordinates), or nullptr if there is none.
  Liveness GetTargetTabStripForPoint(const gfx::Point& point_in_screen,
                                     TabStrip** tab_strip);

  // Returns true if |tabstrip| contains the specified point in screen
  // coordinates.
  bool DoesTabStripContain(TabStrip* tabstrip,
                           const gfx::Point& point_in_screen) const;

  // Returns the DetachPosition given the specified location in screen
  // coordinates.
  DetachPosition GetDetachPosition(const gfx::Point& point_in_screen);

  // Attach the dragged Tab to the specified TabStrip. If |set_capture| is true,
  // the newly attached tabstrip will have capture.
  void Attach(TabStrip* attached_tabstrip,
              const gfx::Point& point_in_screen,
              bool set_capture = true);

  // Detach the dragged Tab from the current TabStrip.
  void Detach(ReleaseCapture release_capture);

  // Detaches the tabs being dragged, creates a new Browser to contain them and
  // runs a nested move loop.
  void DetachIntoNewBrowserAndRunMoveLoop(const gfx::Point& point_in_screen);

  // Runs a nested run loop that handles moving the current
  // Browser. |drag_offset| is the offset from the window origin and is used in
  // calculating the location of the window offset from the cursor while
  // dragging.
  void RunMoveLoop(const gfx::Vector2d& drag_offset);

  // Determines the index to insert tabs at. |dragged_bounds| is the bounds of
  // the tab being dragged, |start| the index of the tab to start looking from.
  // The search proceeds to the end of the strip.
  int GetInsertionIndexFrom(const gfx::Rect& dragged_bounds, int start) const;

  // Like GetInsertionIndexFrom(), but searches backwards from |start| to the
  // beginning of the strip.
  int GetInsertionIndexFromReversed(const gfx::Rect& dragged_bounds,
                                    int start) const;

  // Returns the index where the dragged WebContents should be inserted into
  // |attached_tabstrip_| given the DraggedTabView's bounds |dragged_bounds| in
  // coordinates relative to |attached_tabstrip_| and has had the mirroring
  // transformation applied.
  // NOTE: this is invoked from Attach() before the tabs have been inserted.
  int GetInsertionIndexForDraggedBounds(const gfx::Rect& dragged_bounds) const;

  // Returns true if |dragged_bounds| is close enough to the next stacked tab
  // so that the active tab should be dragged there.
  bool ShouldDragToNextStackedTab(const gfx::Rect& dragged_bounds,
                                  int index) const;

  // Returns true if |dragged_bounds| is close enough to the previous stacked
  // tab so that the active tab should be dragged there.
  bool ShouldDragToPreviousStackedTab(const gfx::Rect& dragged_bounds,
                                      int index) const;

  // Used by GetInsertionIndexForDraggedBounds() when the tabstrip is stacked.
  int GetInsertionIndexForDraggedBoundsStacked(
      const gfx::Rect& dragged_bounds) const;

  // Retrieves the bounds of the dragged tabs relative to the attached TabStrip.
  // |tab_strip_point| is in the attached TabStrip's coordinate system.
  gfx::Rect GetDraggedViewTabStripBounds(const gfx::Point& tab_strip_point);

  // Gets the position of the dragged tabs relative to the attached tab strip
  // with the mirroring transform applied.
  gfx::Point GetAttachedDragPoint(const gfx::Point& point_in_screen);

  // Finds the Tabs within the specified TabStrip that corresponds to the
  // WebContents of the dragged tabs. Returns an empty vector if not attached.
  std::vector<Tab*> GetTabsMatchingDraggedContents(TabStrip* tabstrip);

  // Returns the bounds for the tabs based on the attached tab strip.
  std::vector<gfx::Rect> CalculateBoundsForDraggedTabs();

  // Does the work for EndDrag(). If we actually started a drag and |how_end| is
  // not TAB_DESTROYED then one of EndDrag() or RevertDrag() is invoked.
  void EndDragImpl(EndDragType how_end);

  // Called after the drag ends and |deferred_target_tabstrip_| is not nullptr.
  void PerformDeferredAttach();

  // Reverts a cancelled drag operation.
  void RevertDrag();

  // Reverts the tab at |drag_index| in |drag_data_|.
  void RevertDragAt(size_t drag_index);

  // Selects the dragged tabs in |model|. Does nothing if there are no longer
  // any dragged contents (as happens when a WebContents is deleted out from
  // under us).
  void ResetSelection(TabStripModel* model);

  // Restores |initial_selection_model_| to the |source_tabstrip_|.
  void RestoreInitialSelection();

  // Finishes a successful drag operation.
  void CompleteDrag();

  // Maximizes the attached window.
  void MaximizeAttachedWindow();

  // Returns the bounds (in screen coordinates) of the specified View.
  gfx::Rect GetViewScreenBounds(views::View* tabstrip) const;

  // Hides the frame for the window that contains the TabStrip the current
  // drag session was initiated from.
  void HideFrame();

  void BringWindowUnderPointToFront(const gfx::Point& point_in_screen);

  // Convenience for getting the TabDragData corresponding to the tab the user
  // started dragging.
  TabDragData* source_tab_drag_data() {
    return &(drag_data_[source_tab_index_]);
  }

  // Convenience for |source_tab_drag_data()->contents|.
  content::WebContents* source_dragged_contents() {
    return source_tab_drag_data()->contents;
  }

  // Returns the Widget of the currently attached TabStrip's BrowserView.
  views::Widget* GetAttachedBrowserWidget();

  // Returns true if the tabs were originality one after the other in
  // |source_tabstrip_|.
  bool AreTabsConsecutive();

  // Calculates and returns new bounds for the dragged browser window.
  // Takes into consideration current and restore bounds of |source| tab strip
  // preventing the dragged size from being too small. Positions the new bounds
  // such that the tab that was dragged remains under the |point_in_screen|.
  // Offsets |drag_bounds| if necessary when dragging to the right from the
  // source browser.
  gfx::Rect CalculateDraggedBrowserBounds(TabStrip* source,
                                          const gfx::Point& point_in_screen,
                                          std::vector<gfx::Rect>* drag_bounds);

  // Calculates and returns the dragged bounds for the non-maximize dragged
  // browser window. Taks into consideration the initial drag offset so that
  // the dragged tab remains under the |point_in_screen|.
  gfx::Rect CalculateNonMaximizedDraggedBrowserBounds(
      views::Widget* widget,
      const gfx::Point& point_in_screen);

  // Calculates scaled |drag_bounds| for dragged tabs and sets the tabs bounds.
  // Layout of the tabstrip is performed and a new tabstrip width calculated.
  // When |last_tabstrip_width| is larger than the new tabstrip width the tabs
  // in the attached tabstrip are scaled and the attached browser is positioned
  // such that the tab that was dragged remains under the |point_in_screen|.
  void AdjustBrowserAndTabBoundsForDrag(int last_tabstrip_width,
                                        const gfx::Point& point_in_screen,
                                        std::vector<gfx::Rect>* drag_bounds);

  // Creates and returns a new Browser to handle the drag.
  Browser* CreateBrowserForDrag(TabStrip* source,
                                const gfx::Point& point_in_screen,
                                gfx::Vector2d* drag_offset,
                                std::vector<gfx::Rect>* drag_bounds);

  // Returns the TabStripModel for the specified tabstrip.
  TabStripModel* GetModel(TabStrip* tabstrip) const;

  // Returns the location of the cursor. This is either the location of the
  // mouse or the location of the current touch point.
  gfx::Point GetCursorScreenPoint();

  // Returns the offset from the top left corner of the window to
  // |point_in_screen|.
  gfx::Vector2d GetWindowOffset(const gfx::Point& point_in_screen);

  // Returns true if moving the mouse only changes the visible tabs.
  bool move_only() const {
    return (move_behavior_ == MOVE_VISIBLE_TABS) != 0;
  }

  // Returns the NativeWindow in |window| at the specified point. If
  // |exclude_dragged_view| is true, then the dragged view is not considered.
  Liveness GetLocalProcessWindow(const gfx::Point& screen_point,
                                 bool exclude_dragged_view,
                                 gfx::NativeWindow* window) WARN_UNUSED_RESULT;

  // Sets the dragging info for the current dragged tabstrip. On Chrome OS, the
  // dragging info include two window properties: one is to indicate if the
  // tab-dragging process starts/stops, and the other is to indicate which
  // window initiates the dragging. This function is supposed to be called
  // whenever the dragged tabs are attached to a new tabstrip.
  void SetTabDraggingInfo();

  // Clears the tab dragging info for the current dragged tabstrip. This
  // function is supposed to be called whenever the dragged tabs are detached
  // from the old tabstrip or the tab dragging is ended.
  void ClearTabDraggingInfo();

  // Sets |deferred_target_tabstrip_| and updates its corresponding window
  // property.
  void SetDeferredTargetTabstrip(TabStrip* deferred_target_tabstrip);

  EventSource event_source_;

  // The TabStrip the drag originated from. This is set to null if destroyed
  // during the drag.
  TabStrip* source_tabstrip_;

  // The TabStrip the dragged Tab is currently attached to, or null if the
  // dragged Tab is detached.
  TabStrip* attached_tabstrip_;

#if defined(OS_CHROMEOS)
  // Observe the target TabStrip to attach to after the drag ends. It's only
  // possible to happen in Chrome OS tablet mode, if the dragged tabs are
  // dragged over an overview window, we should wait until the drag ends to
  // attach it.
  std::unique_ptr<DeferredTargetTabstripObserver>
      deferred_target_tabstrip_observer_;
#endif

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

  // Ratio of the x-coordinate of the |source_tab_offset| to the width of the
  // tab.
  float offset_to_width_ratio_;

  // A hint to use when positioning new windows created by detaching Tabs. This
  // is the distance of the mouse from the top left of the dragged tab as if it
  // were the distance of the mouse from the top left of the first tab in the
  // attached TabStrip from the top left of the window.
  gfx::Point window_create_point_;

  // Location of the first tab in the source tabstrip in screen coordinates.
  // This is used to calculate |window_create_point_|.
  gfx::Point first_source_tab_point_;

  // Used to track the view that had focus in the window containing
  // |source_tab_|. This is saved so that focus can be restored properly when
  // a drag begins and ends within this same window.
  std::unique_ptr<views::ViewTracker> old_focused_view_tracker_;

  // The horizontal position of the mouse cursor in screen coordinates at the
  // time of the last re-order event.
  int last_move_screen_loc_;

  // Timer used to bring the window under the cursor to front. If the user
  // stops moving the mouse for a brief time over a browser window, it is
  // brought to front.
  base::OneShotTimer bring_to_front_timer_;

  // Timer used to move the stacked tabs. See comment aboue
  // StartMoveStackedTimerIfNecessary().
  base::OneShotTimer move_stacked_timer_;

  // Did the mouse move enough that we started a drag?
  bool started_drag_;

  // Is the drag active?
  bool active_;

  DragData drag_data_;

  // Index of the source tab in |drag_data_|.
  size_t source_tab_index_;

  // True until MoveAttached() is first invoked.
  bool initial_move_;

  // The selection model before the drag started. See comment above Init() for
  // details.
  ui::ListSelectionModel initial_selection_model_;

  // The selection model of |attached_tabstrip_| before the tabs were attached.
  ui::ListSelectionModel selection_model_before_attach_;

  // Initial x-coordinates of the tabs when the drag started. Only used for
  // touch mode.
  std::vector<int> initial_tab_positions_;

  // What should occur during ConinueDragging when a tab is attempted to be
  // detached.
  DetachBehavior detach_behavior_;

  MoveBehavior move_behavior_;

  // Updated as the mouse is moved when attached. Indicates whether the mouse
  // has ever moved to the left or right. If the tabs are ever detached this
  // is set to kMovedMouseRight | kMovedMouseLeft.
  int mouse_move_direction_;

  // Last location used in screen coordinates.
  gfx::Point last_point_in_screen_;

  // The following are needed when detaching into a browser
  // (|detach_into_browser_| is true).

  // See description above getter.
  bool is_dragging_window_;

  // True if |attached_tabstrip_| is in a browser specifically created for
  // the drag.
  bool is_dragging_new_browser_;

  // True if |source_tabstrip_| was maximized before the drag.
  bool was_source_maximized_;

  // True if |source_tabstrip_| was in immersive fullscreen before the drag.
  bool was_source_fullscreen_;

  // True if the initial drag resulted in restoring the window (because it was
  // maximized).
  bool did_restore_window_;

  EndRunLoopBehavior end_run_loop_behavior_;

  // If true, we're waiting for a move loop to complete.
  bool waiting_for_run_loop_to_exit_;

  // The TabStrip to attach to after the move loop completes.
  TabStrip* tab_strip_to_attach_to_after_exit_;

  // Non-null for the duration of RunMoveLoop.
  views::Widget* move_loop_widget_;

  // See description above getter.
  bool is_mutating_;

  // |attach_x_| and |attach_index_| are set to the x-coordinate of the mouse
  // (in terms of the tabstrip) and the insertion index at the time tabs are
  // dragged into a new browser (attached). They are used to ensure we don't
  // shift the tabs around in the wrong direction. The two are only valid if
  // |attach_index_| is not -1.
  // See comment around use for more details.
  int attach_x_;
  int attach_index_;

  std::unique_ptr<EscapeTracker> escape_tracker_;

  std::unique_ptr<WindowFinder> window_finder_;

#if defined(USE_AURA)
  aura::Env* env_ = nullptr;
#endif

  base::WeakPtrFactory<TabDragController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabDragController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
