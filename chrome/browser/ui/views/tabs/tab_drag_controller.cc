// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/stacked_tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/window_properties.h"               // nogncheck
#include "ash/public/interfaces/window_state_type.mojom.h"  // nogncheck
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/wm/core/coordinate_conversion.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"  // nogncheck
#include "ui/aura/window.h"  // nogncheck
#include "ui/wm/core/window_modality_controller.h"  // nogncheck
#endif

using content::OpenURLParams;
using content::WebContents;

// If non-null there is a drag underway.
static TabDragController* g_tab_drag_controller = NULL;

namespace {

// Delay, in ms, during dragging before we bring a window to front.
const int kBringToFrontDelay = 750;

// Initial delay before moving tabs when the dragged tab is close to the edge of
// the stacked tabs.
const int kMoveAttachedInitialDelay = 600;

// Delay for moving tabs after the initial delay has passed.
const int kMoveAttachedSubsequentDelay = 300;

const int kHorizontalMoveThreshold = 16;  // DIPs.

// Distance from the next/previous stacked before before we consider the tab
// close enough to trigger moving.
const int kStackedDistance = 36;

// A dragged window is forced to be a bit smaller than maximized bounds during a
// drag. This prevents the dragged browser widget from getting maximized at
// creation and makes it easier to drag tabs out of a restored window that had
// maximized size.
const int kMaximizedWindowInset = 10;  // DIPs.

// Given the bounds of a dragged tab, return the X coordinate to use for
// computing where in the strip to insert/move the tab.
int GetDraggedX(const gfx::Rect& dragged_bounds) {
  return dragged_bounds.x() + TabStyle::GetTabInternalPadding().left();
}

#if defined(OS_CHROMEOS)

// Returns the aura::Window which stores the window properties for tab-dragging.
// It should return the root window when WindowService is used, since it is
// corresponded with a widget in Ash.
aura::Window* GetWindowForTabDraggingProperties(const TabStrip* tab_strip) {
  if (!tab_strip)
    return nullptr;
  aura::Window* window = tab_strip->GetWidget()->GetNativeWindow();
  if (features::IsUsingWindowService())
    return window->GetRootWindow();
  return window;
}

// Returns true if |tab_strip| browser window is snapped.
bool IsSnapped(const TabStrip* tab_strip) {
  DCHECK(tab_strip);
  aura::Window* window = GetWindowForTabDraggingProperties(tab_strip);
  ash::mojom::WindowStateType type =
      window->GetProperty(ash::kTabDroppedWindowStateTypeKey);
  if (type == ash::mojom::WindowStateType::DEFAULT)
    type = window->GetProperty(ash::kWindowStateTypeKey);
  return type == ash::mojom::WindowStateType::LEFT_SNAPPED ||
         type == ash::mojom::WindowStateType::RIGHT_SNAPPED;
}

// In Chrome OS tablet mode, when dragging a tab/tabs around, the desired
// browser bounds during dragging is one-fourth of the workspace bounds.
gfx::Rect GetDraggedBrowserBoundsInTabletMode(aura::Window* window) {
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  gfx::Rect bounds(window->bounds());
  bounds.set_width(work_area.width() / 2);
  bounds.set_height(work_area.height() / 2);
  return bounds;
}

// Store the current window bounds if we're in Chrome OS tablet mode and tab
// dragging is allowed on browser windows.
void StoreCurrentDraggedBrowserBoundsInTabletMode(
    aura::Window* window,
    const gfx::Rect& bounds_in_screen) {
  if (TabletModeClient::Get()->tablet_mode_enabled() &&
      base::FeatureList::IsEnabled(ash::features::kDragTabsInTabletMode)) {
    // The bounds that is stored in ash::kRestoreBoundsOverrideKey will be used
    // by DragDetails to calculate the window bounds during dragging in tablet
    // mode.
    window->SetProperty(ash::kRestoreBoundsOverrideKey,
                        new gfx::Rect(bounds_in_screen));
  }
}

// Returns true if |tabstrip| is currently showing in overview mode in Chrome
// OS.
bool IsShowingInOverview(TabStrip* tabstrip) {
  return tabstrip && GetWindowForTabDraggingProperties(tabstrip)->GetProperty(
                         ash::kIsShowingInOverviewKey);
}

// Returns true if we should attach the dragged tabs into |target_tabstrip|
// after the drag ends. Currently it only happens on Chrome OS, when the dragged
// tabs are dragged over an overview window, we should not try to attach it
// to the overview window during dragging, but should wait to do so until the
// drag ends.
bool ShouldAttachOnEnd(TabStrip* target_tabstrip) {
  return IsShowingInOverview(target_tabstrip);
}

// Returns true if |tabstrip| can detach from the current tabstrip and attach
// into another eligible browser window's tabstrip.
bool CanDetachFromTabStrip(TabStrip* tabstrip) {
  return tabstrip && GetWindowForTabDraggingProperties(tabstrip)->GetProperty(
                         ash::kCanAttachToAnotherWindowKey);
}

#else
bool IsSnapped(const TabStrip* tab_strip) {
  return false;
}

bool IsShowingInOverview(TabStrip* tabstrip) {
  return false;
}

bool ShouldAttachOnEnd(TabStrip* target_tabstrip) {
  return false;
}

bool CanDetachFromTabStrip(TabStrip* tabstrip) {
  return true;
}

#endif  // #if defined(OS_CHROMEOS)

#if defined(USE_AURA)
gfx::NativeWindow GetModalTransient(gfx::NativeWindow window) {
  return wm::GetModalTransient(window);
}
#else
gfx::NativeWindow GetModalTransient(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return NULL;
}
#endif

// Returns true if |bounds| contains the y-coordinate |y|. The y-coordinate
// of |bounds| is adjusted by |vertical_adjustment|.
bool DoesRectContainVerticalPointExpanded(
    const gfx::Rect& bounds,
    int vertical_adjustment,
    int y) {
  int upper_threshold = bounds.bottom() + vertical_adjustment;
  int lower_threshold = bounds.y() - vertical_adjustment;
  return y >= lower_threshold && y <= upper_threshold;
}

// Adds |x_offset| to all the rectangles in |rects|.
void OffsetX(int x_offset, std::vector<gfx::Rect>* rects) {
  if (x_offset == 0)
    return;

  for (size_t i = 0; i < rects->size(); ++i)
    (*rects)[i].set_x((*rects)[i].x() + x_offset);
}

}  // namespace

// EscapeTracker installs an event monitor and runs a callback when it receives
// the escape key.
class EscapeTracker : public ui::EventObserver {
 public:
  EscapeTracker(base::OnceClosure callback, gfx::NativeWindow context)
      : escape_callback_(std::move(callback)) {
    event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
        this, context, {ui::ET_KEY_PRESSED});
  }
  ~EscapeTracker() override = default;

 private:
  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    if (event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE && escape_callback_)
      std::move(escape_callback_).Run();
  }

  base::OnceClosure escape_callback_;
  std::unique_ptr<views::EventMonitor> event_monitor_;

  DISALLOW_COPY_AND_ASSIGN(EscapeTracker);
};

TabDragController::TabDragData::TabDragData()
    : contents(NULL),
      source_model_index(-1),
      attached_tab(NULL),
      pinned(false) {
}

TabDragController::TabDragData::~TabDragData() {
}

TabDragController::TabDragData::TabDragData(TabDragData&&) = default;

#if defined(OS_CHROMEOS)

// The class to track the current deferred target tabstrip and also to observe
// its native window's property ash::kIsDeferredTabDraggingTargetWindowKey.
// The reason we need to observe the window property is the property might be
// cleared outside of TabDragController (i.e. by ash), and we should update the
// tracked deferred target tabstrip in this case.
class TabDragController::DeferredTargetTabstripObserver
    : public aura::WindowObserver {
 public:
  DeferredTargetTabstripObserver() = default;
  ~DeferredTargetTabstripObserver() override {
    if (deferred_target_tabstrip_) {
      GetWindowForTabDraggingProperties(deferred_target_tabstrip_)
          ->RemoveObserver(this);
      deferred_target_tabstrip_ = nullptr;
    }
  }

  void SetDeferredTargetTabstrip(TabStrip* deferred_target_tabstrip) {
    if (deferred_target_tabstrip_ == deferred_target_tabstrip)
      return;

    // Clear the window property on the previous |deferred_target_tabstrip_|.
    if (deferred_target_tabstrip_) {
      aura::Window* old_window =
          GetWindowForTabDraggingProperties(deferred_target_tabstrip_);
      old_window->RemoveObserver(this);
      old_window->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);
    }

    deferred_target_tabstrip_ = deferred_target_tabstrip;

    // Set the window property on the new |deferred_target_tabstrip_|.
    if (deferred_target_tabstrip_) {
      aura::Window* new_window =
          GetWindowForTabDraggingProperties(deferred_target_tabstrip_);
      new_window->SetProperty(ash::kIsDeferredTabDraggingTargetWindowKey, true);
      new_window->AddObserver(this);
    }
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window,
              GetWindowForTabDraggingProperties(deferred_target_tabstrip_));

    if (key == ash::kIsDeferredTabDraggingTargetWindowKey &&
        !window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
      SetDeferredTargetTabstrip(nullptr);
    }

    // else do nothing. currently it's only possible that ash clears the window
    // property, but doesn't set the window property.
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window,
              GetWindowForTabDraggingProperties(deferred_target_tabstrip_));
    SetDeferredTargetTabstrip(nullptr);
  }

  TabStrip* deferred_target_tabstrip() { return deferred_target_tabstrip_; }

 private:
  TabStrip* deferred_target_tabstrip_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DeferredTargetTabstripObserver);
};

#endif

///////////////////////////////////////////////////////////////////////////////
// TabDragController, public:

// static
const int TabDragController::kTouchVerticalDetachMagnetism = 50;

// static
const int TabDragController::kVerticalDetachMagnetism = 15;

TabDragController::TabDragController()
    : event_source_(EVENT_SOURCE_MOUSE),
      source_tabstrip_(NULL),
      attached_tabstrip_(NULL),
      can_release_capture_(true),
      offset_to_width_ratio_(0),
      old_focused_view_tracker_(std::make_unique<views::ViewTracker>()),
      last_move_screen_loc_(0),
      started_drag_(false),
      active_(true),
      source_tab_index_(std::numeric_limits<size_t>::max()),
      initial_move_(true),
      detach_behavior_(DETACHABLE),
      move_behavior_(REORDER),
      mouse_move_direction_(0),
      is_dragging_window_(false),
      is_dragging_new_browser_(false),
      was_source_maximized_(false),
      was_source_fullscreen_(false),
      did_restore_window_(false),
      end_run_loop_behavior_(END_RUN_LOOP_STOP_DRAGGING),
      waiting_for_run_loop_to_exit_(false),
      tab_strip_to_attach_to_after_exit_(NULL),
      move_loop_widget_(NULL),
      is_mutating_(false),
      attach_x_(-1),
      attach_index_(-1),
      weak_factory_(this) {
  g_tab_drag_controller = this;
}

TabDragController::~TabDragController() {
  if (g_tab_drag_controller == this)
    g_tab_drag_controller = NULL;

  if (move_loop_widget_)
    move_loop_widget_->RemoveObserver(this);

  if (source_tabstrip_)
    GetModel(source_tabstrip_)->RemoveObserver(this);

  if (event_source_ == EVENT_SOURCE_TOUCH) {
    TabStrip* capture_tabstrip =
        attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
    capture_tabstrip->GetWidget()->ReleaseCapture();
  }
}

void TabDragController::Init(TabStrip* source_tabstrip,
                             Tab* source_tab,
                             const std::vector<Tab*>& tabs,
                             const gfx::Point& mouse_offset,
                             int source_tab_offset,
                             ui::ListSelectionModel initial_selection_model,
                             MoveBehavior move_behavior,
                             EventSource event_source) {
  DCHECK(!tabs.empty());
  DCHECK(base::ContainsValue(tabs, source_tab));
  source_tabstrip_ = source_tabstrip;
  was_source_maximized_ = source_tabstrip->GetWidget()->IsMaximized();
  was_source_fullscreen_ = source_tabstrip->GetWidget()->IsFullscreen();
  // Do not release capture when transferring capture between widgets on:
  // - Desktop Linux
  //     Mouse capture is not synchronous on desktop Linux. Chrome makes
  //     transferring capture between widgets without releasing capture appear
  //     synchronous on desktop Linux, so use that.
  // - Chrome OS
  //     Releasing capture on Ash cancels gestures so avoid it.
#if defined(OS_LINUX)
  can_release_capture_ = false;
#endif
  start_point_in_screen_ = gfx::Point(source_tab_offset, mouse_offset.y());
  views::View::ConvertPointToScreen(source_tab, &start_point_in_screen_);
  event_source_ = event_source;
  mouse_offset_ = mouse_offset;
  move_behavior_ = move_behavior;
  last_point_in_screen_ = start_point_in_screen_;
  last_move_screen_loc_ = start_point_in_screen_.x();
  initial_tab_positions_ = source_tabstrip->GetTabXCoordinates();

  GetModel(source_tabstrip_)->AddObserver(this);

  drag_data_.resize(tabs.size());
  for (size_t i = 0; i < tabs.size(); ++i)
    InitTabDragData(tabs[i], &(drag_data_[i]));
  source_tab_index_ =
      std::find(tabs.begin(), tabs.end(), source_tab) - tabs.begin();

  // Listen for Esc key presses.
  escape_tracker_ = std::make_unique<EscapeTracker>(
      base::BindOnce(&TabDragController::EndDrag, weak_factory_.GetWeakPtr(),
                     END_DRAG_CANCEL),
      source_tabstrip_->GetWidget()->GetNativeWindow());

  if (source_tab->width() > 0) {
    offset_to_width_ratio_ = static_cast<float>(
        source_tab->GetMirroredXInView(source_tab_offset)) /
        static_cast<float>(source_tab->width());
  }
  InitWindowCreatePoint();
  initial_selection_model_ = std::move(initial_selection_model);

  // Gestures don't automatically do a capture. We don't allow multiple drags at
  // the same time, so we explicitly capture.
  if (event_source == EVENT_SOURCE_TOUCH)
    source_tabstrip_->GetWidget()->SetCapture(source_tabstrip_);

#if defined(USE_AURA)
  env_ = source_tabstrip_->GetWidget()->GetNativeWindow()->env();
#endif

#if defined(OS_CHROMEOS)
  if (TabletModeClient::Get()->tablet_mode_enabled() &&
      !base::FeatureList::IsEnabled(ash::features::kDragTabsInTabletMode)) {
    detach_behavior_ = NOT_DETACHABLE;
  }
#endif
  window_finder_ = WindowFinder::Create(
      event_source, source_tabstrip->GetWidget()->GetNativeWindow());
}

// static
bool TabDragController::IsAttachedTo(const TabStrip* tab_strip) {
  return (g_tab_drag_controller && g_tab_drag_controller->active() &&
          g_tab_drag_controller->attached_tabstrip() == tab_strip);
}

// static
bool TabDragController::IsActive() {
  return g_tab_drag_controller && g_tab_drag_controller->active();
}

void TabDragController::SetMoveBehavior(MoveBehavior behavior) {
  if (started_drag())
    return;

  move_behavior_ = behavior;
}

bool TabDragController::IsDraggingTab(content::WebContents* contents) {
  for (auto& drag_data : drag_data_) {
    if (drag_data.contents == contents)
      return true;
  }
  return false;
}

void TabDragController::Drag(const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::Drag",
               "point_in_screen", point_in_screen.ToString());

  bring_to_front_timer_.Stop();
  move_stacked_timer_.Stop();

  if (waiting_for_run_loop_to_exit_)
    return;

  if (!started_drag_) {
    if (!CanStartDrag(point_in_screen))
      return;  // User hasn't dragged far enough yet.

    // On windows SaveFocus() may trigger a capture lost, which destroys us.
    {
      base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
      SaveFocus();
      if (!ref)
        return;
    }
    started_drag_ = true;
    Attach(source_tabstrip_, gfx::Point());
    if (static_cast<int>(drag_data_.size()) ==
        GetModel(source_tabstrip_)->count()) {
      views::Widget* widget = GetAttachedBrowserWidget();
      gfx::Rect new_bounds;
      if (was_source_maximized_ || was_source_fullscreen_) {
        did_restore_window_ = true;
        // When all tabs in a maximized browser are dragged the browser gets
        // restored during the drag and maximized back when the drag ends.
        const int last_tabstrip_width = attached_tabstrip_->GetTabAreaWidth();
        std::vector<gfx::Rect> drag_bounds = CalculateBoundsForDraggedTabs();
        OffsetX(GetAttachedDragPoint(point_in_screen).x(), &drag_bounds);
        new_bounds = CalculateDraggedBrowserBounds(
            source_tabstrip_, point_in_screen, &drag_bounds);
        new_bounds.Offset(-widget->GetRestoredBounds().x() +
                          point_in_screen.x() -
                          mouse_offset_.x(), 0);
        widget->SetVisibilityChangedAnimationsEnabled(false);
        widget->Restore();
        widget->SetBounds(new_bounds);
        AdjustBrowserAndTabBoundsForDrag(last_tabstrip_width,
                                         point_in_screen,
                                         &drag_bounds);
        widget->SetVisibilityChangedAnimationsEnabled(true);
      } else {
        new_bounds =
            CalculateNonMaximizedDraggedBrowserBounds(widget, point_in_screen);
        widget->SetBounds(new_bounds);
      }

#if defined(OS_CHROMEOS)
      StoreCurrentDraggedBrowserBoundsInTabletMode(widget->GetNativeWindow(),
                                                   new_bounds);
#endif

      RunMoveLoop(GetWindowOffset(point_in_screen));
      return;
    }
  }

  if (ContinueDragging(point_in_screen) == Liveness::DELETED)
    return;
}

void TabDragController::EndDrag(EndDragReason reason) {
  TRACE_EVENT0("views", "TabDragController::EndDrag");

  // If we're dragging a window ignore capture lost since it'll ultimately
  // trigger the move loop to end and we'll revert the drag when RunMoveLoop()
  // finishes.
  if (reason == END_DRAG_CAPTURE_LOST && is_dragging_window_)
    return;

#if defined(OS_CHROMEOS)
  // It's possible that in Chrome OS we defer the windows that are showing in
  // overview to attach into during dragging. If so we need to attach the
  // dragged tabs to it first.
  if (reason == END_DRAG_COMPLETE && deferred_target_tabstrip_observer_)
    PerformDeferredAttach();

  // It's also possible that we need to merge the dragged tabs back into the
  // source window even if the dragged tabs is dragged away from the source
  // window.
  if (source_tabstrip_ &&
      GetWindowForTabDraggingProperties(source_tabstrip_)
          ->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
    GetWindowForTabDraggingProperties(source_tabstrip_)
        ->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);
    reason = END_DRAG_CANCEL;
  }
#endif

  EndDragImpl(reason != END_DRAG_COMPLETE && source_tabstrip_ ?
              CANCELED : NORMAL);
}

void TabDragController::InitTabDragData(Tab* tab,
                                        TabDragData* drag_data) {
  TRACE_EVENT0("views", "TabDragController::InitTabDragData");
  drag_data->source_model_index =
      source_tabstrip_->GetModelIndexOfTab(tab);
  drag_data->contents = GetModel(source_tabstrip_)->GetWebContentsAt(
      drag_data->source_model_index);
  drag_data->pinned = source_tabstrip_->IsTabPinned(tab);
}

void TabDragController::OnWidgetBoundsChanged(views::Widget* widget,
                                              const gfx::Rect& new_bounds) {
  TRACE_EVENT1("views", "TabDragController::OnWidgetBoundsChanged",
               "new_bounds", new_bounds.ToString());

  Drag(GetCursorScreenPoint());
}

void TabDragController::TabStripEmpty() {
  GetModel(source_tabstrip_)->RemoveObserver(this);
  // NULL out source_tabstrip_ so that we don't attempt to add back to it (in
  // the case of a revert).
  source_tabstrip_ = nullptr;
#if defined(OS_CHROMEOS)
  // Also update the source window info for the current dragged window.
  if (attached_tabstrip_) {
    GetWindowForTabDraggingProperties(attached_tabstrip_)
        ->ClearProperty(ash::kTabDraggingSourceWindowKey);
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, private:

void TabDragController::InitWindowCreatePoint() {
  // window_create_point_ is only used in CompleteDrag() (through
  // GetWindowCreatePoint() to get the start point of the docked window) when
  // the attached_tabstrip_ is NULL and all the window's related bound
  // information are obtained from source_tabstrip_. So, we need to get the
  // first_tab based on source_tabstrip_, not attached_tabstrip_. Otherwise,
  // the window_create_point_ is not in the correct coordinate system. Please
  // refer to http://crbug.com/6223 comment #15 for detailed information.
  views::View* first_tab = source_tabstrip_->tab_at(0);
  views::View::ConvertPointToWidget(first_tab, &first_source_tab_point_);
  window_create_point_ = first_source_tab_point_;
  window_create_point_.Offset(mouse_offset_.x(), mouse_offset_.y());
}

gfx::Point TabDragController::GetWindowCreatePoint(
    const gfx::Point& origin) const {
  // If the cursor is outside the monitor area, move it inside. For example,
  // dropping a tab onto the task bar on Windows produces this situation.
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestPoint(origin).work_area();
  gfx::Point create_point(origin);
  if (!work_area.IsEmpty()) {
    if (create_point.x() < work_area.x())
      create_point.set_x(work_area.x());
    else if (create_point.x() > work_area.right())
      create_point.set_x(work_area.right());
    if (create_point.y() < work_area.y())
      create_point.set_y(work_area.y());
    else if (create_point.y() > work_area.bottom())
      create_point.set_y(work_area.bottom());
  }
  return gfx::Point(create_point.x() - window_create_point_.x(),
                    create_point.y() - window_create_point_.y());
}

void TabDragController::SaveFocus() {
  DCHECK(source_tabstrip_);
  old_focused_view_tracker_->SetView(
      source_tabstrip_->GetFocusManager()->GetFocusedView());
  source_tabstrip_->GetFocusManager()->ClearFocus();
  // WARNING: we may have been deleted.
}

void TabDragController::RestoreFocus() {
  if (attached_tabstrip_ != source_tabstrip_) {
    if (is_dragging_new_browser_) {
      content::WebContents* active_contents = source_dragged_contents();
      if (active_contents && !active_contents->FocusLocationBarByDefault())
        active_contents->Focus();
    }
    return;
  }
  views::View* old_focused_view = old_focused_view_tracker_->view();
  if (!old_focused_view)
    return;
  old_focused_view->GetFocusManager()->SetFocusedView(old_focused_view);
}

bool TabDragController::CanStartDrag(const gfx::Point& point_in_screen) const {
  // Determine if the mouse has moved beyond a minimum elasticity distance in
  // any direction from the starting point.
  static const int kMinimumDragDistance = 10;
  int x_offset = abs(point_in_screen.x() - start_point_in_screen_.x());
  int y_offset = abs(point_in_screen.y() - start_point_in_screen_.y());
  return sqrt(pow(static_cast<float>(x_offset), 2) +
              pow(static_cast<float>(y_offset), 2)) > kMinimumDragDistance;
}

TabDragController::Liveness TabDragController::ContinueDragging(
    const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::ContinueDragging",
               "point_in_screen", point_in_screen.ToString());

  DCHECK(attached_tabstrip_);

  TabStrip* target_tabstrip = source_tabstrip_;
  if (detach_behavior_ == DETACHABLE &&
      GetTargetTabStripForPoint(point_in_screen, &target_tabstrip) ==
          Liveness::DELETED) {
    return Liveness::DELETED;
  }

  // The dragged tabs may not be able to attach into |target_tabstrip| during
  // dragging if the window accociated with |target_tabstrip| is currently
  // showing in overview mode in Chrome OS, in this case we defer attaching into
  // it till the drag ends and reset |target_tabstrip| here.
  if (ShouldAttachOnEnd(target_tabstrip)) {
    SetDeferredTargetTabstrip(target_tabstrip);
    target_tabstrip = is_dragging_window_ ? attached_tabstrip_ : nullptr;
  } else {
    SetDeferredTargetTabstrip(nullptr);
  }

  bool tab_strip_changed = (target_tabstrip != attached_tabstrip_);

  if (attached_tabstrip_) {
    int move_delta = point_in_screen.x() - last_point_in_screen_.x();
    if (move_delta > 0)
      mouse_move_direction_ |= kMovedMouseRight;
    else if (move_delta < 0)
      mouse_move_direction_ |= kMovedMouseLeft;
  }
  last_point_in_screen_ = point_in_screen;

  if (tab_strip_changed) {
    is_dragging_new_browser_ = false;
    did_restore_window_ = false;
    if (DragBrowserToNewTabStrip(target_tabstrip, point_in_screen) ==
        DRAG_BROWSER_RESULT_STOP) {
      return Liveness::ALIVE;
    }
  }
  if (is_dragging_window_) {
    bring_to_front_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kBringToFrontDelay),
        base::Bind(&TabDragController::BringWindowUnderPointToFront,
                   base::Unretained(this), point_in_screen));
  }

  if (!is_dragging_window_ && attached_tabstrip_) {
    if (move_only()) {
      DragActiveTabStacked(point_in_screen);
    } else {
      MoveAttached(point_in_screen);
      if (tab_strip_changed) {
        // Move the corresponding window to the front. We do this after the
        // move as on windows activate triggers a synchronous paint.
        attached_tabstrip_->GetWidget()->Activate();
      }
    }
  }
  return Liveness::ALIVE;
}

TabDragController::DragBrowserResultType
TabDragController::DragBrowserToNewTabStrip(TabStrip* target_tabstrip,
                                            const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::DragBrowserToNewTabStrip",
               "point_in_screen", point_in_screen.ToString());

  if (!target_tabstrip) {
    DetachIntoNewBrowserAndRunMoveLoop(point_in_screen);
    return DRAG_BROWSER_RESULT_STOP;
  }

#if defined(USE_AURA)
  // Only Aura windows are gesture consumers.
  gfx::NativeView attached_native_view =
      GetAttachedBrowserWidget()->GetNativeView();
#if defined(OS_CHROMEOS)
  // When using WindowService, the touch events for the window move have
  // happened on the root window, so the transfer should happen from the root of
  // the currently attached window to the target.
  if (features::IsUsingWindowService())
    attached_native_view = attached_native_view->GetRootWindow();
#endif
  GetAttachedBrowserWidget()->GetGestureRecognizer()->TransferEventsTo(
      attached_native_view, target_tabstrip->GetWidget()->GetNativeView(),
      ui::TransferTouchesBehavior::kDontCancel);
#endif

  if (is_dragging_window_) {
    // ReleaseCapture() is going to result in calling back to us (because it
    // results in a move). That'll cause all sorts of problems.  Reset the
    // observer so we don't get notified and process the event.
#if defined(OS_CHROMEOS)
    move_loop_widget_->RemoveObserver(this);
    move_loop_widget_ = nullptr;
#endif  // OS_CHROMEOS
    views::Widget* browser_widget = GetAttachedBrowserWidget();
    // Need to release the drag controller before starting the move loop as it's
    // going to trigger capture lost, which cancels drag.
    attached_tabstrip_->ReleaseDragController();
    target_tabstrip->OwnDragController(this);
    // Disable animations so that we don't see a close animation on aero.
    browser_widget->SetVisibilityChangedAnimationsEnabled(false);
    if (can_release_capture_)
      browser_widget->ReleaseCapture();
    else
      target_tabstrip->GetWidget()->SetCapture(attached_tabstrip_);

#if !defined(OS_LINUX)
    // EndMoveLoop is going to snap the window back to its original location.
    // Hide it so users don't see this. Hiding a window in Linux aura causes
    // it to lose capture so skip it.
    browser_widget->Hide();
#endif
    browser_widget->EndMoveLoop();

    // Ideally we would always swap the tabs now, but on non-ash Windows, it
    // seems that running the move loop implicitly activates the window when
    // done, leading to all sorts of flicker. So, on non-ash Windows, instead
    // we process the move after the loop completes. But on chromeos, we can
    // do tab swapping now to avoid the tab flashing issue
    // (crbug.com/116329).
    if (can_release_capture_) {
      tab_strip_to_attach_to_after_exit_ = target_tabstrip;
    } else {
      is_dragging_window_ = false;
      Detach(DONT_RELEASE_CAPTURE);
      Attach(target_tabstrip, point_in_screen);
      // Move the tabs into position.
      MoveAttached(point_in_screen);
      attached_tabstrip_->GetWidget()->Activate();
    }

    waiting_for_run_loop_to_exit_ = true;
    end_run_loop_behavior_ = END_RUN_LOOP_CONTINUE_DRAGGING;
    return DRAG_BROWSER_RESULT_STOP;
  }
  Detach(DONT_RELEASE_CAPTURE);
  Attach(target_tabstrip, point_in_screen);
  return DRAG_BROWSER_RESULT_CONTINUE;
}

void TabDragController::DragActiveTabStacked(
    const gfx::Point& point_in_screen) {
  if (attached_tabstrip_->tab_count() !=
      static_cast<int>(initial_tab_positions_.size()))
    return;  // TODO: should cancel drag if this happens.

  int delta = point_in_screen.x() - start_point_in_screen_.x();
  attached_tabstrip_->DragActiveTabStacked(initial_tab_positions_, delta);
}

void TabDragController::MoveAttachedToNextStackedIndex(
    const gfx::Point& point_in_screen) {
  int index = attached_tabstrip_->touch_layout_->active_index();
  if (index + 1 >= attached_tabstrip_->tab_count())
    return;

  GetModel(attached_tabstrip_)->MoveSelectedTabsTo(index + 1);
  StartMoveStackedTimerIfNecessary(point_in_screen,
                                   kMoveAttachedSubsequentDelay);
}

void TabDragController::MoveAttachedToPreviousStackedIndex(
    const gfx::Point& point_in_screen) {
  int index = attached_tabstrip_->touch_layout_->active_index();
  if (index <= attached_tabstrip_->GetPinnedTabCount())
    return;

  GetModel(attached_tabstrip_)->MoveSelectedTabsTo(index - 1);
  StartMoveStackedTimerIfNecessary(point_in_screen,
                                   kMoveAttachedSubsequentDelay);
}

void TabDragController::MoveAttached(const gfx::Point& point_in_screen) {
  DCHECK(attached_tabstrip_);
  DCHECK(!is_dragging_window_);

  gfx::Point dragged_view_point = GetAttachedDragPoint(point_in_screen);

  // Determine the horizontal move threshold. This is dependent on the width
  // of tabs. The smaller the tabs compared to the standard size, the smaller
  // the threshold.
  int threshold = kHorizontalMoveThreshold;
  if (!attached_tabstrip_->touch_layout_.get()) {
    double ratio =
        static_cast<double>(attached_tabstrip_->current_inactive_width()) /
        TabStyle::GetStandardWidth();
    threshold = gfx::ToRoundedInt(ratio * kHorizontalMoveThreshold);
  }
  // else case: touch tabs never shrink.

  std::vector<Tab*> tabs(drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    tabs[i] = drag_data_[i].attached_tab;

  bool did_layout = false;
  // Update the model, moving the WebContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter) or if this the first move and the tabs are not consecutive.
  if ((abs(point_in_screen.x() - last_move_screen_loc_) > threshold ||
        (initial_move_ && !AreTabsConsecutive()))) {
    TabStripModel* attached_model = GetModel(attached_tabstrip_);
    int to_index = GetInsertionIndexForDraggedBounds(
        GetDraggedViewTabStripBounds(dragged_view_point));
    bool do_move = true;
    // While dragging within a tabstrip the expectation is the insertion index
    // is based on the left edge of the tabs being dragged. OTOH when dragging
    // into a new tabstrip (attaching) the expectation is the insertion index is
    // based on the cursor. This proves problematic as insertion may change the
    // size of the tabs, resulting in the index calculated before the insert
    // differing from the index calculated after the insert. To alleviate this
    // the index is chosen before insertion, and subsequently a new index is
    // only used once the mouse moves enough such that the index changes based
    // on the direction the mouse moved relative to |attach_x_| (smaller
    // x-coordinate should yield a smaller index or larger x-coordinate yields a
    // larger index).
    if (attach_index_ != -1) {
      gfx::Point tab_strip_point(point_in_screen);
      views::View::ConvertPointFromScreen(attached_tabstrip_, &tab_strip_point);
      const int new_x =
          attached_tabstrip_->GetMirroredXInView(tab_strip_point.x());
      if (new_x < attach_x_)
        to_index = std::min(to_index, attach_index_);
      else
        to_index = std::max(to_index, attach_index_);
      if (to_index != attach_index_)
        attach_index_ = -1;  // Once a valid move is detected, don't constrain.
      else
        do_move = false;
    }
    if (do_move) {
      WebContents* last_contents = drag_data_.back().contents;
      int index_of_last_item =
          attached_model->GetIndexOfWebContents(last_contents);
      if (initial_move_) {
        // TabStrip determines if the tabs needs to be animated based on model
        // position. This means we need to invoke LayoutDraggedTabsAt before
        // changing the model.
        attached_tabstrip_->LayoutDraggedTabsAt(
            tabs, source_tab_drag_data()->attached_tab, dragged_view_point,
            initial_move_);
        did_layout = true;
      }
      attached_model->MoveSelectedTabsTo(to_index);

      // Move may do nothing in certain situations (such as when dragging pinned
      // tabs). Make sure the tabstrip actually changed before updating
      // last_move_screen_loc_.
      if (index_of_last_item !=
          attached_model->GetIndexOfWebContents(last_contents)) {
        last_move_screen_loc_ = point_in_screen.x();
      }
    }
  }

  if (!did_layout) {
    attached_tabstrip_->LayoutDraggedTabsAt(
        tabs, source_tab_drag_data()->attached_tab, dragged_view_point,
        initial_move_);
  }

  StartMoveStackedTimerIfNecessary(point_in_screen, kMoveAttachedInitialDelay);

  initial_move_ = false;
}

void TabDragController::StartMoveStackedTimerIfNecessary(
    const gfx::Point& point_in_screen,
    int delay_ms) {
  DCHECK(attached_tabstrip_);

  StackedTabStripLayout* touch_layout = attached_tabstrip_->touch_layout_.get();
  if (!touch_layout)
    return;

  gfx::Point dragged_view_point = GetAttachedDragPoint(point_in_screen);
  gfx::Rect bounds = GetDraggedViewTabStripBounds(dragged_view_point);
  int index = touch_layout->active_index();
  if (ShouldDragToNextStackedTab(bounds, index)) {
    move_stacked_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(delay_ms),
        base::Bind(&TabDragController::MoveAttachedToNextStackedIndex,
                   base::Unretained(this), point_in_screen));
  } else if (ShouldDragToPreviousStackedTab(bounds, index)) {
    move_stacked_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(delay_ms),
        base::Bind(&TabDragController::MoveAttachedToPreviousStackedIndex,
                   base::Unretained(this), point_in_screen));
  }
}

TabDragController::DetachPosition TabDragController::GetDetachPosition(
    const gfx::Point& point_in_screen) {
  DCHECK(attached_tabstrip_);
  gfx::Point attached_point(point_in_screen);
  views::View::ConvertPointFromScreen(attached_tabstrip_, &attached_point);
  if (attached_point.x() < attached_tabstrip_->TabStartX())
    return DETACH_BEFORE;
  if (attached_point.x() >= attached_tabstrip_->TabDragAreaEndX())
    return DETACH_AFTER;
  return DETACH_ABOVE_OR_BELOW;
}

TabDragController::Liveness TabDragController::GetTargetTabStripForPoint(
    const gfx::Point& point_in_screen,
    TabStrip** tab_strip) {
  *tab_strip = nullptr;
  TRACE_EVENT1("views", "TabDragController::GetTargetTabStripForPoint",
               "point_in_screen", point_in_screen.ToString());

  // Do not change the current attached tabstrip if it's not allowed to detach
  // from the current tabstrip and attach into another window's tabstrip.
  if (attached_tabstrip_ && !CanDetachFromTabStrip(attached_tabstrip_)) {
    *tab_strip = attached_tabstrip_;
    return Liveness::ALIVE;
  }

  if (move_only() && attached_tabstrip_) {
    // move_only() is intended for touch, in which case we only want to detach
    // if the touch point moves significantly in the vertical distance.
    gfx::Rect tabstrip_bounds = GetViewScreenBounds(attached_tabstrip_);
    if (DoesRectContainVerticalPointExpanded(tabstrip_bounds,
                                             kTouchVerticalDetachMagnetism,
                                             point_in_screen.y())) {
      *tab_strip = attached_tabstrip_;
      return Liveness::ALIVE;
    }
  }
  gfx::NativeWindow local_window;
  const Liveness state = GetLocalProcessWindow(
      point_in_screen, is_dragging_window_, &local_window);
  if (state == Liveness::DELETED)
    return Liveness::DELETED;

  // Do not allow dragging into a window with a modal dialog, it causes a weird
  // behavior.  See crbug.com/336691
  if (!GetModalTransient(local_window)) {
    TabStrip* result = GetTabStripForWindow(local_window);
    if (ShouldAttachOnEnd(result)) {
      // No need to check if the specified screen point is within the bounds of
      // the tabstrip as arriving here we know that the window is currently
      // showing in overview mode in Chrome OS and its bounds contain the
      // specified screen point, and these two conditions are enough for a
      // window to be a valid target window to attach the dragged tabs.
      *tab_strip = result;
      return Liveness::ALIVE;
    } else if (result && DoesTabStripContain(result, point_in_screen)) {
      *tab_strip = result;
      return Liveness::ALIVE;
    }
  }

  *tab_strip = is_dragging_window_ ? attached_tabstrip_ : nullptr;
  return Liveness::ALIVE;
}

TabStrip* TabDragController::GetTabStripForWindow(gfx::NativeWindow window) {
  if (!window)
    return NULL;
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  // We don't allow drops on windows that don't have tabstrips.
  if (!browser_view ||
      !browser_view->browser()->SupportsWindowFeature(
          Browser::FEATURE_TABSTRIP))
    return NULL;

  TabStrip* other_tabstrip = browser_view->tabstrip();
  TabStrip* tab_strip =
      attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
  DCHECK(tab_strip);

  return other_tabstrip->controller()->IsCompatibleWith(tab_strip) ?
      other_tabstrip : NULL;
}

bool TabDragController::DoesTabStripContain(
    TabStrip* tabstrip,
    const gfx::Point& point_in_screen) const {
  // Make sure the specified screen point is actually within the bounds of the
  // specified tabstrip...
  gfx::Rect tabstrip_bounds = GetViewScreenBounds(tabstrip);
  const int x_in_strip = point_in_screen.x() - tabstrip_bounds.x();
  return (x_in_strip >= tabstrip->TabStartX()) &&
         (x_in_strip < tabstrip->TabDragAreaEndX()) &&
         DoesRectContainVerticalPointExpanded(
             tabstrip_bounds, kVerticalDetachMagnetism, point_in_screen.y());
}

void TabDragController::Attach(TabStrip* attached_tabstrip,
                               const gfx::Point& point_in_screen,
                               bool set_capture) {
  TRACE_EVENT1("views", "TabDragController::Attach",
               "point_in_screen", point_in_screen.ToString());

  DCHECK(!attached_tabstrip_);  // We should already have detached by the time
                                // we get here.

  attached_tabstrip_ = attached_tabstrip;

  std::vector<Tab*> tabs =
      GetTabsMatchingDraggedContents(attached_tabstrip_);

  if (tabs.empty()) {
    // Transitioning from detached to attached to a new tabstrip. Add tabs to
    // the new model.

    selection_model_before_attach_ = attached_tabstrip->GetSelectionModel();

    // Inserting counts as a move. We don't want the tabs to jitter when the
    // user moves the tab immediately after attaching it.
    last_move_screen_loc_ = point_in_screen.x();

    // Figure out where to insert the tab based on the bounds of the dragged
    // representation and the ideal bounds of the other Tabs already in the
    // strip. ("ideal bounds" are stable even if the Tabs' actual bounds are
    // changing due to animation).
    gfx::Point tab_strip_point(point_in_screen);
    views::View::ConvertPointFromScreen(attached_tabstrip_, &tab_strip_point);
    tab_strip_point.set_x(
        attached_tabstrip_->GetMirroredXInView(tab_strip_point.x()));
    tab_strip_point.Offset(0, -mouse_offset_.y());
    int index = GetInsertionIndexForDraggedBounds(
        GetDraggedViewTabStripBounds(tab_strip_point));
    attach_index_ = index;
    attach_x_ = tab_strip_point.x();
    base::AutoReset<bool> setter(&is_mutating_, true);
    for (size_t i = 0; i < drag_data_.size(); ++i) {
      int add_types = TabStripModel::ADD_NONE;
      if (attached_tabstrip_->touch_layout_.get()) {
        // StackedTabStripLayout positions relative to the active tab, if we
        // don't add the tab as active things bounce around.
        DCHECK_EQ(1u, drag_data_.size());
        add_types |= TabStripModel::ADD_ACTIVE;
      }
      if (drag_data_[i].pinned)
        add_types |= TabStripModel::ADD_PINNED;
      GetModel(attached_tabstrip_)
          ->InsertWebContentsAt(
              index + i, std::move(drag_data_[i].owned_contents), add_types);

      // If a sad tab is showing, the SadTabView needs to be updated.
      SadTabHelper* sad_tab_helper =
          SadTabHelper::FromWebContents(drag_data_[i].contents);
      if (sad_tab_helper)
        sad_tab_helper->ReinstallInWebView();
    }

    tabs = GetTabsMatchingDraggedContents(attached_tabstrip_);
  }
  DCHECK_EQ(tabs.size(), drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    drag_data_[i].attached_tab = tabs[i];

  ResetSelection(GetModel(attached_tabstrip_));

  // This should be called after ResetSelection() in order to generate
  // bounds correctly. http://crbug.com/836004
  attached_tabstrip_->StartedDraggingTabs(tabs);

  // The size of the dragged tab may have changed. Adjust the x offset so that
  // ratio of mouse_offset_ to original width is maintained.
  std::vector<Tab*> tabs_to_source(tabs);
  tabs_to_source.erase(tabs_to_source.begin() + source_tab_index_ + 1,
                       tabs_to_source.end());
  int new_x = attached_tabstrip_->GetSizeNeededForTabs(tabs_to_source) -
              tabs[source_tab_index_]->width() +
              gfx::ToRoundedInt(offset_to_width_ratio_ *
                                tabs[source_tab_index_]->width());
  mouse_offset_.set_x(new_x);

  // Transfer ownership of us to the new tabstrip as well as making sure the
  // window has capture. This is important so that if activation changes the
  // drag isn't prematurely canceled.
  if (set_capture)
    attached_tabstrip_->GetWidget()->SetCapture(attached_tabstrip_);
  attached_tabstrip_->OwnDragController(this);
  SetTabDraggingInfo();
}

void TabDragController::Detach(ReleaseCapture release_capture) {
  TRACE_EVENT1("views", "TabDragController::Detach",
               "release_capture", release_capture);

  attach_index_ = -1;

  // When the user detaches we assume they want to reorder.
  move_behavior_ = REORDER;

  // Release ownership of the drag controller and mouse capture. When we
  // reattach ownership is transfered.
  attached_tabstrip_->ReleaseDragController();
  if (release_capture == RELEASE_CAPTURE)
    attached_tabstrip_->GetWidget()->ReleaseCapture();

  mouse_move_direction_ = kMovedMouseLeft | kMovedMouseRight;

  std::vector<gfx::Rect> drag_bounds = CalculateBoundsForDraggedTabs();
  TabStripModel* attached_model = GetModel(attached_tabstrip_);
  std::vector<TabRendererData> tab_data;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    tab_data.push_back(drag_data_[i].attached_tab->data());
    int index = attached_model->GetIndexOfWebContents(drag_data_[i].contents);
    DCHECK_NE(-1, index);

    // Hide the tab so that the user doesn't see it animate closed.
    drag_data_[i].attached_tab->SetVisible(false);
    drag_data_[i].attached_tab->set_detached();
    drag_data_[i].owned_contents = attached_model->DetachWebContentsAt(index);

    // Detaching may end up deleting the tab, drop references to it.
    drag_data_[i].attached_tab = NULL;
  }

  // If we've removed the last Tab from the TabStrip, hide the frame now.
  if (!attached_model->empty()) {
    if (!selection_model_before_attach_.empty() &&
        selection_model_before_attach_.active() >= 0 &&
        selection_model_before_attach_.active() < attached_model->count()) {
      // Restore the selection.
      attached_model->SetSelectionFromModel(selection_model_before_attach_);
    } else if (attached_tabstrip_ == source_tabstrip_ &&
               !initial_selection_model_.empty()) {
      RestoreInitialSelection();
    }
  }

  ClearTabDraggingInfo();
  attached_tabstrip_->DraggedTabsDetached();
  attached_tabstrip_ = NULL;
}

void TabDragController::DetachIntoNewBrowserAndRunMoveLoop(
    const gfx::Point& point_in_screen) {
  if (GetModel(attached_tabstrip_)->count() ==
      static_cast<int>(drag_data_.size())) {
    // All the tabs in a browser are being dragged but all the tabs weren't
    // initially being dragged. For this to happen the user would have to
    // start dragging a set of tabs, the other tabs close, then detach.
    RunMoveLoop(GetWindowOffset(point_in_screen));
    return;
  }

  const int last_tabstrip_width = attached_tabstrip_->GetTabAreaWidth();
  std::vector<gfx::Rect> drag_bounds = CalculateBoundsForDraggedTabs();
  OffsetX(GetAttachedDragPoint(point_in_screen).x(), &drag_bounds);

  gfx::Vector2d drag_offset;
  Browser* browser = CreateBrowserForDrag(
      attached_tabstrip_, point_in_screen, &drag_offset, &drag_bounds);

  BrowserView* dragged_browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::Widget* dragged_widget = dragged_browser_view->GetWidget();

#if defined(USE_AURA)
  // Only Aura windows are gesture consumers.
  views::Widget* attached_widget = attached_tabstrip_->GetWidget();
  // Unlike DragBrowserToNewTabStrip, this does not have to special-handle
  // IsUsingWindowServices(), since DesktopWIndowTreeHostMus takes care of it.
  attached_widget->GetGestureRecognizer()->TransferEventsTo(
      attached_widget->GetNativeView(), dragged_widget->GetNativeView(),
      ui::TransferTouchesBehavior::kDontCancel);
#endif

  Detach(can_release_capture_ ? RELEASE_CAPTURE : DONT_RELEASE_CAPTURE);

  dragged_widget->SetVisibilityChangedAnimationsEnabled(false);
  Attach(dragged_browser_view->tabstrip(), gfx::Point());
  AdjustBrowserAndTabBoundsForDrag(last_tabstrip_width,
                                   point_in_screen,
                                   &drag_bounds);
  browser->window()->Show();
  dragged_widget->SetVisibilityChangedAnimationsEnabled(true);
  // Activate may trigger a focus loss, destroying us.
  {
    base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
    browser->window()->Activate();
    if (!ref)
      return;
  }
  RunMoveLoop(drag_offset);
}

void TabDragController::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  // If the user drags the whole window we'll assume they are going to attach to
  // another window and therefore want to reorder.
  move_behavior_ = REORDER;

  move_loop_widget_ = GetAttachedBrowserWidget();
  DCHECK(move_loop_widget_);
  move_loop_widget_->AddObserver(this);
  is_dragging_window_ = true;
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  if (can_release_capture_) {
    // Running the move loop releases mouse capture, which triggers destroying
    // the drag loop. Release mouse capture now while the DragController is not
    // owned by the TabStrip.
    attached_tabstrip_->ReleaseDragController();
    attached_tabstrip_->GetWidget()->ReleaseCapture();
    attached_tabstrip_->OwnDragController(this);
  }
  const views::Widget::MoveLoopSource move_loop_source =
      event_source_ == EVENT_SOURCE_MOUSE ?
      views::Widget::MOVE_LOOP_SOURCE_MOUSE :
      views::Widget::MOVE_LOOP_SOURCE_TOUCH;
  const views::Widget::MoveLoopEscapeBehavior escape_behavior =
      is_dragging_new_browser_ ?
          views::Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_HIDE :
          views::Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_DONT_HIDE;
  views::Widget::MoveLoopResult result =
      move_loop_widget_->RunMoveLoop(
          drag_offset, move_loop_source, escape_behavior);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::NotificationService::NoDetails());

  if (!ref)
    return;
  if (move_loop_widget_) {
    move_loop_widget_->RemoveObserver(this);
    move_loop_widget_ = nullptr;
  }
  is_dragging_window_ = false;
  waiting_for_run_loop_to_exit_ = false;
  if (end_run_loop_behavior_ == END_RUN_LOOP_CONTINUE_DRAGGING) {
    end_run_loop_behavior_ = END_RUN_LOOP_STOP_DRAGGING;
    if (tab_strip_to_attach_to_after_exit_) {
      gfx::Point point_in_screen(GetCursorScreenPoint());
      Detach(DONT_RELEASE_CAPTURE);
      Attach(tab_strip_to_attach_to_after_exit_, point_in_screen);
      // Move the tabs into position.
      MoveAttached(point_in_screen);
      attached_tabstrip_->GetWidget()->Activate();
      // Activate may trigger a focus loss, destroying us.
      if (!ref)
        return;
      tab_strip_to_attach_to_after_exit_ = NULL;
    }
    DCHECK(attached_tabstrip_);
    attached_tabstrip_->GetWidget()->SetCapture(attached_tabstrip_);
  } else if (active_) {
    EndDrag(result == views::Widget::MOVE_LOOP_CANCELED ?
            END_DRAG_CANCEL : END_DRAG_COMPLETE);
  }
}

int TabDragController::GetInsertionIndexFrom(const gfx::Rect& dragged_bounds,
                                             int start) const {
  const int last_tab = attached_tabstrip_->tab_count() - 1;
  const int dragged_x = GetDraggedX(dragged_bounds);
  if (start < 0 || start > last_tab ||
      dragged_x < attached_tabstrip_->ideal_bounds(start).x())
    return -1;

  for (int i = start; i <= last_tab; ++i) {
    if (dragged_x < attached_tabstrip_->ideal_bounds(i).CenterPoint().x())
      return i;
  }

  return (dragged_x < attached_tabstrip_->ideal_bounds(last_tab).right()) ?
      (last_tab + 1) : -1;
}

int TabDragController::GetInsertionIndexFromReversed(
    const gfx::Rect& dragged_bounds,
    int start) const {
  const int dragged_x = GetDraggedX(dragged_bounds);
  if (start < 0 || start >= attached_tabstrip_->tab_count() ||
      dragged_x >= attached_tabstrip_->ideal_bounds(start).right())
    return -1;

  for (int i = start; i >= 0; --i) {
    if (dragged_x >= attached_tabstrip_->ideal_bounds(i).CenterPoint().x())
      return i + 1;
  }

  return (dragged_x >= attached_tabstrip_->ideal_bounds(0).x()) ? 0 : -1;
}

int TabDragController::GetInsertionIndexForDraggedBounds(
    const gfx::Rect& dragged_bounds) const {
  // If the strip has no tabs, the only position to insert at is 0.
  const int tab_count = attached_tabstrip_->tab_count();
  if (!tab_count)
    return 0;

  int index = -1;
  if (attached_tabstrip_->touch_layout_.get()) {
    index = GetInsertionIndexForDraggedBoundsStacked(dragged_bounds);
    if (index != -1) {
      // Only move the tab to the left/right if the user actually moved the
      // mouse that way. This is necessary as tabs with stacked tabs
      // before/after them have multiple drag positions.
      int active_index = attached_tabstrip_->touch_layout_->active_index();
      if ((index < active_index &&
           (mouse_move_direction_ & kMovedMouseLeft) == 0) ||
          (index > active_index &&
           (mouse_move_direction_ & kMovedMouseRight) == 0)) {
        index = active_index;
      }
    }
  } else {
    index = GetInsertionIndexFrom(dragged_bounds, 0);
  }
  if (index == -1) {
    const int last_tab_right =
        attached_tabstrip_->ideal_bounds(tab_count - 1).right();
    index = (dragged_bounds.right() > last_tab_right) ? tab_count : 0;
  }

  const Tab* last_visible_tab = attached_tabstrip_->GetLastVisibleTab();
  int last_insertion_point = last_visible_tab ?
      (attached_tabstrip_->GetModelIndexOfTab(last_visible_tab) + 1) : 0;
  if (drag_data_[0].attached_tab) {
    // We're not in the process of attaching, so clamp the insertion point to
    // keep it within the visible region.
    last_insertion_point = std::max(
        0, last_insertion_point - static_cast<int>(drag_data_.size()));
  }

  // Ensure the first dragged tab always stays in the visible index range.
  return std::min(index, last_insertion_point);
}

bool TabDragController::ShouldDragToNextStackedTab(
    const gfx::Rect& dragged_bounds,
    int index) const {
  if (index + 1 >= attached_tabstrip_->tab_count() ||
      !attached_tabstrip_->touch_layout_->IsStacked(index + 1) ||
      (mouse_move_direction_ & kMovedMouseRight) == 0)
    return false;

  int active_x = attached_tabstrip_->ideal_bounds(index).x();
  int next_x = attached_tabstrip_->ideal_bounds(index + 1).x();
  int mid_x = std::min(next_x - kStackedDistance,
                       active_x + (next_x - active_x) / 4);
  return GetDraggedX(dragged_bounds) >= mid_x;
}

bool TabDragController::ShouldDragToPreviousStackedTab(
    const gfx::Rect& dragged_bounds,
    int index) const {
  if (index - 1 < attached_tabstrip_->GetPinnedTabCount() ||
      !attached_tabstrip_->touch_layout_->IsStacked(index - 1) ||
      (mouse_move_direction_ & kMovedMouseLeft) == 0)
    return false;

  int active_x = attached_tabstrip_->ideal_bounds(index).x();
  int previous_x = attached_tabstrip_->ideal_bounds(index - 1).x();
  int mid_x = std::max(previous_x + kStackedDistance,
                       active_x - (active_x - previous_x) / 4);
  return GetDraggedX(dragged_bounds) <= mid_x;
}

int TabDragController::GetInsertionIndexForDraggedBoundsStacked(
    const gfx::Rect& dragged_bounds) const {
  StackedTabStripLayout* touch_layout = attached_tabstrip_->touch_layout_.get();
  int active_index = touch_layout->active_index();
  // Search from the active index to the front of the tabstrip. Do this as tabs
  // overlap each other from the active index.
  int index = GetInsertionIndexFromReversed(dragged_bounds, active_index);
  if (index != active_index)
    return index;
  if (index == -1)
    return GetInsertionIndexFrom(dragged_bounds, active_index + 1);

  // The position to drag to corresponds to the active tab. If the next/previous
  // tab is stacked, then shorten the distance used to determine insertion
  // bounds. We do this as GetInsertionIndexFrom() uses the bounds of the
  // tabs. When tabs are stacked the next/previous tab is on top of the tab.
  if (active_index + 1 < attached_tabstrip_->tab_count() &&
      touch_layout->IsStacked(active_index + 1)) {
    index = GetInsertionIndexFrom(dragged_bounds, active_index + 1);
    if (index == -1 && ShouldDragToNextStackedTab(dragged_bounds, active_index))
      index = active_index + 1;
    else if (index == -1)
      index = active_index;
  } else if (ShouldDragToPreviousStackedTab(dragged_bounds, active_index)) {
    index = active_index - 1;
  }
  return index;
}

gfx::Rect TabDragController::GetDraggedViewTabStripBounds(
    const gfx::Point& tab_strip_point) {
  // attached_tab is NULL when inserting into a new tabstrip.
  // TODO(pkasting): This assumes there is just one tab being dragged, which is
  // wrong when dragging multiple tabs; need to check all of |drag_data_|.
  if (source_tab_drag_data()->attached_tab) {
    return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                     source_tab_drag_data()->attached_tab->width(),
                     source_tab_drag_data()->attached_tab->height());
  }

  return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                   attached_tabstrip_->current_active_width(),
                   GetLayoutConstant(TAB_HEIGHT));
}

gfx::Point TabDragController::GetAttachedDragPoint(
    const gfx::Point& point_in_screen) {
  DCHECK(attached_tabstrip_);  // The tab must be attached.

  gfx::Point tab_loc(point_in_screen);
  views::View::ConvertPointFromScreen(attached_tabstrip_, &tab_loc);
  const int x =
      attached_tabstrip_->GetMirroredXInView(tab_loc.x()) - mouse_offset_.x();

  // TODO: consider caching this.
  std::vector<Tab*> attached_tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i)
    attached_tabs.push_back(drag_data_[i].attached_tab);
  const int size = attached_tabstrip_->GetSizeNeededForTabs(attached_tabs);
  const int min_x = attached_tabstrip_->TabStartX();
  const int max_x = attached_tabstrip_->TabDragAreaEndX() - size;
  return gfx::Point(base::ClampToRange(x, min_x, max_x), 0);
}

std::vector<Tab*> TabDragController::GetTabsMatchingDraggedContents(
    TabStrip* tabstrip) {
  TabStripModel* model = GetModel(attached_tabstrip_);
  std::vector<Tab*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    int model_index = model->GetIndexOfWebContents(drag_data_[i].contents);
    if (model_index == TabStripModel::kNoTab)
      return std::vector<Tab*>();
    tabs.push_back(tabstrip->tab_at(model_index));
  }
  return tabs;
}

std::vector<gfx::Rect> TabDragController::CalculateBoundsForDraggedTabs() {
  std::vector<Tab*> attached_tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i)
    attached_tabs.push_back(drag_data_[i].attached_tab);
  return attached_tabstrip_->CalculateBoundsForDraggedTabs(attached_tabs);
}

void TabDragController::EndDragImpl(EndDragType type) {
  DCHECK(active_);
  active_ = false;

  bring_to_front_timer_.Stop();
  move_stacked_timer_.Stop();

  if (move_loop_widget_) {
    // This function is only called when the drag is ending. At this point we
    // don't care about any subsequent moves to the widget, so we remove the
    // observer. If we didn't do this we could get told the widget moved and
    // attempt to do the wrong thing.
    move_loop_widget_->RemoveObserver(this);
    move_loop_widget_ = nullptr;
  }

  if (is_dragging_window_) {
    waiting_for_run_loop_to_exit_ = true;

    // End the nested drag loop.
    GetAttachedBrowserWidget()->EndMoveLoop();
  }

  if (type != TAB_DESTROYED) {
    // We only finish up the drag if we were actually dragging. If start_drag_
    // is false, the user just clicked and released and didn't move the mouse
    // enough to trigger a drag.
    if (started_drag_) {
      // After the drag ends, if |attached_tabstrip_| is showing in overview
      // mode, do not restore focus, otherwise overview mode may be ended
      // unexpectly because of the window activation.
      if (!IsShowingInOverview(attached_tabstrip_))
        RestoreFocus();

      if (type == CANCELED)
        RevertDrag();
      else
        CompleteDrag();
    }
  } else if (drag_data_.size() > 1) {
    initial_selection_model_.Clear();
    if (started_drag_)
      RevertDrag();
  }  // else case the only tab we were dragging was deleted. Nothing to do.

  // Clear tab dragging info after the complete/revert as CompleteDrag() may
  // need to use some of the properties.
  ClearTabDraggingInfo();

  // Clear out drag data so we don't attempt to do anything with it.
  drag_data_.clear();

  TabStrip* owning_tabstrip =
      attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
  owning_tabstrip->DestroyDragController();
}

void TabDragController::PerformDeferredAttach() {
#if defined(OS_CHROMEOS)
  TabStrip* deferred_target_tabstrip =
      deferred_target_tabstrip_observer_->deferred_target_tabstrip();
  if (!deferred_target_tabstrip)
    return;

  DCHECK_NE(deferred_target_tabstrip, attached_tabstrip_);

  // |is_dragging_new_browser_| needs to be reset here since after this function
  // is called, the browser window that was specially created for the dragged
  // tab(s) will be destroyed.
  is_dragging_new_browser_ = false;
  // |did_restore_window_| is only set to be true if the dragged window is the
  // source window and the source window was maximized or fullscreen before the
  // drag starts. It also needs to be reset to false here otherwise after this
  // function is called, the newly attached window may be maximized unexpectedly
  // after the drag ends.
  did_restore_window_ = false;

  TabStrip* target_tabstrip = deferred_target_tabstrip;
  SetDeferredTargetTabstrip(nullptr);
  deferred_target_tabstrip_observer_.reset();

  // GetCursorScreenPoint() needs to be called before Detach() is called as
  // GetCursorScreenPoint() may use the current attached tabstrip to get the
  // touch event position but Detach() sets attached tabstrip to nullptr.
  const gfx::Point current_screen_point = GetCursorScreenPoint();
  Detach(DONT_RELEASE_CAPTURE);
  // If we're attaching the dragged tabs to an overview window's tabstrip, the
  // tabstrip should not have focus.
  Attach(target_tabstrip, current_screen_point, /*set_capture=*/false);
#endif
}

void TabDragController::RevertDrag() {
  std::vector<Tab*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents) {
      // Contents is NULL if a tab was destroyed while the drag was under way.
      tabs.push_back(drag_data_[i].attached_tab);
      RevertDragAt(i);
    }
  }

  if (attached_tabstrip_) {
    if (did_restore_window_)
      MaximizeAttachedWindow();
    if (attached_tabstrip_ == source_tabstrip_) {
      source_tabstrip_->StoppedDraggingTabs(
          tabs, initial_tab_positions_, move_behavior_ == MOVE_VISIBLE_TABS,
          false);
    } else {
      attached_tabstrip_->DraggedTabsDetached();
    }
  }

  if (initial_selection_model_.empty())
    ResetSelection(GetModel(source_tabstrip_));
  else
    GetModel(source_tabstrip_)->SetSelectionFromModel(initial_selection_model_);

  if (source_tabstrip_)
    source_tabstrip_->GetWidget()->Activate();
}

void TabDragController::ResetSelection(TabStripModel* model) {
  DCHECK(model);
  ui::ListSelectionModel selection_model;
  bool has_one_valid_tab = false;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    // |contents| is NULL if a tab was deleted out from under us.
    if (drag_data_[i].contents) {
      int index = model->GetIndexOfWebContents(drag_data_[i].contents);
      DCHECK_NE(-1, index);
      selection_model.AddIndexToSelection(index);
      if (!has_one_valid_tab || i == source_tab_index_) {
        // Reset the active/lead to the first tab. If the source tab is still
        // valid we'll reset these again later on.
        selection_model.set_active(index);
        selection_model.set_anchor(index);
        has_one_valid_tab = true;
      }
    }
  }
  if (!has_one_valid_tab)
    return;

  model->SetSelectionFromModel(selection_model);
}

void TabDragController::RestoreInitialSelection() {
  // First time detaching from the source tabstrip. Reset selection model to
  // initial_selection_model_. Before resetting though we have to remove all
  // the tabs from initial_selection_model_ as it was created with the tabs
  // still there.
  ui::ListSelectionModel selection_model = initial_selection_model_;
  for (DragData::const_reverse_iterator i(drag_data_.rbegin());
       i != drag_data_.rend(); ++i) {
    selection_model.DecrementFrom(i->source_model_index);
  }
  // We may have cleared out the selection model. Only reset it if it
  // contains something.
  if (selection_model.empty())
    return;

  // The anchor/active may have been among the tabs that were dragged out. Force
  // the anchor/active to be valid.
  if (selection_model.anchor() == ui::ListSelectionModel::kUnselectedIndex)
    selection_model.set_anchor(selection_model.selected_indices()[0]);
  if (selection_model.active() == ui::ListSelectionModel::kUnselectedIndex)
    selection_model.set_active(selection_model.selected_indices()[0]);
  GetModel(source_tabstrip_)->SetSelectionFromModel(selection_model);
}

void TabDragController::RevertDragAt(size_t drag_index) {
  DCHECK(started_drag_);
  DCHECK(source_tabstrip_);

  base::AutoReset<bool> setter(&is_mutating_, true);
  TabDragData* data = &(drag_data_[drag_index]);
  if (attached_tabstrip_) {
    int index =
        GetModel(attached_tabstrip_)->GetIndexOfWebContents(data->contents);
    if (attached_tabstrip_ != source_tabstrip_) {
      // The Tab was inserted into another TabStrip. We need to put it back
      // into the original one.
      std::unique_ptr<content::WebContents> detached_web_contents =
          GetModel(attached_tabstrip_)->DetachWebContentsAt(index);
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      GetModel(source_tabstrip_)
          ->InsertWebContentsAt(data->source_model_index,
                                std::move(detached_web_contents),
                                (data->pinned ? TabStripModel::ADD_PINNED : 0));
    } else {
      // The Tab was moved within the TabStrip where the drag was initiated.
      // Move it back to the starting location.
      GetModel(source_tabstrip_)->MoveWebContentsAt(
          index, data->source_model_index, false);
    }
  } else {
    // The Tab was detached from the TabStrip where the drag began, and has not
    // been attached to any other TabStrip. We need to put it back into the
    // source TabStrip.
    GetModel(source_tabstrip_)
        ->InsertWebContentsAt(data->source_model_index,
                              std::move(data->owned_contents),
                              (data->pinned ? TabStripModel::ADD_PINNED : 0));
  }
}

void TabDragController::CompleteDrag() {
  DCHECK(started_drag_);

  if (attached_tabstrip_) {
    if (is_dragging_new_browser_ || did_restore_window_) {
      if (IsSnapped(attached_tabstrip_)) {
        was_source_maximized_ = false;
        was_source_fullscreen_ = false;
      }

      // If source window was maximized - maximize the new window as well.
      if (was_source_maximized_ || was_source_fullscreen_)
        MaximizeAttachedWindow();
    }
    attached_tabstrip_->StoppedDraggingTabs(
        GetTabsMatchingDraggedContents(attached_tabstrip_),
        initial_tab_positions_,
        move_behavior_ == MOVE_VISIBLE_TABS,
        true);
  } else {
    // Compel the model to construct a new window for the detached
    // WebContentses.
    views::Widget* widget = source_tabstrip_->GetWidget();
    gfx::Rect window_bounds(widget->GetRestoredBounds());
    window_bounds.set_origin(GetWindowCreatePoint(last_point_in_screen_));

    base::AutoReset<bool> setter(&is_mutating_, true);

    std::vector<TabStripModelDelegate::NewStripContents> contentses;
    for (size_t i = 0; i < drag_data_.size(); ++i) {
      TabStripModelDelegate::NewStripContents item;
      item.web_contents = std::move(drag_data_[i].owned_contents);
      item.add_types = drag_data_[i].pinned ? TabStripModel::ADD_PINNED
                                            : TabStripModel::ADD_NONE;
      contentses.push_back(std::move(item));
    }

    Browser* new_browser =
        GetModel(source_tabstrip_)
            ->delegate()
            ->CreateNewStripWithContents(std::move(contentses), window_bounds,
                                         widget->IsMaximized());
    ResetSelection(new_browser->tab_strip_model());
    new_browser->window()->Show();
  }
}

void TabDragController::MaximizeAttachedWindow() {
  GetAttachedBrowserWidget()->Maximize();
#if defined(OS_CHROMEOS)
  if (was_source_fullscreen_) {
    // In fullscreen mode it is only possible to get here if the source
    // was in "immersive fullscreen" mode, so toggle it back on.
    BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
        GetAttachedBrowserWidget()->GetNativeWindow());
    DCHECK(browser_view);
    if (!browser_view->IsFullscreen())
      chrome::ToggleFullscreenMode(browser_view->browser());
  }
#endif
}

gfx::Rect TabDragController::GetViewScreenBounds(
    views::View* view) const {
  gfx::Point view_topleft;
  views::View::ConvertPointToScreen(view, &view_topleft);
  gfx::Rect view_screen_bounds = view->GetLocalBounds();
  view_screen_bounds.Offset(view_topleft.x(), view_topleft.y());
  return view_screen_bounds;
}

void TabDragController::BringWindowUnderPointToFront(
    const gfx::Point& point_in_screen) {
  gfx::NativeWindow window;
  if (GetLocalProcessWindow(point_in_screen, true, &window) ==
      Liveness::DELETED) {
    return;
  }

  // Only bring browser windows to front - only windows with a TabStrip can
  // be tab drag targets.
  if (!GetTabStripForWindow(window))
    return;

  if (window) {
    views::Widget* widget_window = views::Widget::GetWidgetForNativeWindow(
        window);
    if (!widget_window)
      return;

#if defined(OS_CHROMEOS)
    // TODO(varkha): The code below ensures that the phantom drag widget
    // is shown on top of browser windows. The code should be moved to ash/
    // and the phantom should be able to assert its top-most state on its own.
    // One strategy would be for DragWindowController to
    // be able to observe stacking changes to the phantom drag widget's
    // siblings in order to keep it on top. One way is to implement a
    // notification that is sent to a window parent's observers when a
    // stacking order is changed among the children of that same parent.
    // Note that OnWindowStackingChanged is sent only to the child that is the
    // argument of one of the Window::StackChildX calls and not to all its
    // siblings affected by the stacking change.
    aura::Window* browser_window = widget_window->GetNativeView();
    // Find a topmost non-popup window and stack the recipient browser above
    // it in order to avoid stacking the browser window on top of the phantom
    // drag widget created by DragWindowController in a second display.
    for (aura::Window::Windows::const_reverse_iterator it =
             browser_window->parent()->children().rbegin();
         it != browser_window->parent()->children().rend(); ++it) {
      // If the iteration reached the recipient browser window then it is
      // already topmost and it is safe to return with no stacking change.
      if (*it == browser_window)
        return;
      if ((*it)->type() != aura::client::WINDOW_TYPE_POPUP) {
        widget_window->StackAbove(*it);
        break;
      }
    }
#else
    widget_window->StackAtTop();
#endif

    // The previous call made the window appear on top of the dragged window,
    // move the dragged window to the front.
    if (is_dragging_window_)
      attached_tabstrip_->GetWidget()->StackAtTop();
  }
}

TabStripModel* TabDragController::GetModel(TabStrip* tabstrip) const {
  return static_cast<BrowserTabStripController*>(tabstrip->controller())->
      model();
}

views::Widget* TabDragController::GetAttachedBrowserWidget() {
  return attached_tabstrip_->GetWidget();
}

bool TabDragController::AreTabsConsecutive() {
  for (size_t i = 1; i < drag_data_.size(); ++i) {
    if (drag_data_[i - 1].source_model_index + 1 !=
        drag_data_[i].source_model_index) {
      return false;
    }
  }
  return true;
}

gfx::Rect TabDragController::CalculateDraggedBrowserBounds(
    TabStrip* source,
    const gfx::Point& point_in_screen,
    std::vector<gfx::Rect>* drag_bounds) {
  gfx::Point center(0, source->height() / 2);
  views::View::ConvertPointToWidget(source, &center);
  gfx::Rect new_bounds(source->GetWidget()->GetRestoredBounds());

  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestPoint(last_point_in_screen_)
                            .work_area();
  if (new_bounds.size().width() >= work_area.size().width() &&
      new_bounds.size().height() >= work_area.size().height()) {
    new_bounds = work_area;
    new_bounds.Inset(kMaximizedWindowInset, kMaximizedWindowInset,
                     kMaximizedWindowInset, kMaximizedWindowInset);
    // Behave as if the |source| was maximized at the start of a drag since this
    // is consistent with a browser window creation logic in case of windows
    // that are as large as the |work_area|.
    was_source_maximized_ = true;
  }

  if (source->GetWidget()->IsMaximized()) {
    // If the restore bounds is really small, we don't want to honor it
    // (dragging a really small window looks wrong), instead make sure the new
    // window is at least 50% the size of the old.
    const gfx::Size max_size(
        source->GetWidget()->GetWindowBoundsInScreen().size());
    new_bounds.set_width(
        std::max(max_size.width() / 2, new_bounds.width()));
    new_bounds.set_height(
        std::max(max_size.height() / 2, new_bounds.height()));
  }

#if defined(OS_CHROMEOS)
  if (TabletModeClient::Get()->tablet_mode_enabled() &&
      base::FeatureList::IsEnabled(ash::features::kDragTabsInTabletMode)) {
    new_bounds = GetDraggedBrowserBoundsInTabletMode(
        source->GetWidget()->GetNativeWindow());
  }
#endif

  new_bounds.set_y(point_in_screen.y() - center.y());
  switch (GetDetachPosition(point_in_screen)) {
    case DETACH_BEFORE:
      new_bounds.set_x(point_in_screen.x() - center.x());
      new_bounds.Offset(-mouse_offset_.x(), 0);
      break;
    case DETACH_AFTER: {
      gfx::Point right_edge(source->width(), 0);
      views::View::ConvertPointToWidget(source, &right_edge);
      new_bounds.set_x(point_in_screen.x() - right_edge.x());
      new_bounds.Offset(drag_bounds->back().right() - mouse_offset_.x(), 0);
      OffsetX(-drag_bounds->front().x(), drag_bounds);
      break;
    }
    default:
      break; // Nothing to do for DETACH_ABOVE_OR_BELOW.
  }

  // Account for the extra space above the tabstrip on restored windows versus
  // maximized windows.
  if (source->GetWidget()->IsMaximized()) {
    const auto* frame_view = static_cast<BrowserNonClientFrameView*>(
        source->GetWidget()->non_client_view()->frame_view());
    new_bounds.Offset(
        0, frame_view->GetTopInset(false) - frame_view->GetTopInset(true));
  }
  return new_bounds;
}

gfx::Rect TabDragController::CalculateNonMaximizedDraggedBrowserBounds(
    views::Widget* widget,
    const gfx::Point& point_in_screen) {
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
#if defined(OS_CHROMEOS)
  if (TabletModeClient::Get()->tablet_mode_enabled() &&
      base::FeatureList::IsEnabled(ash::features::kDragTabsInTabletMode)) {
    bounds = GetDraggedBrowserBoundsInTabletMode(widget->GetNativeWindow());
  }
#endif

  // The user has to move the mouse some amount of pixels before the drag
  // starts. Offset the window by this amount so that the relative offset
  // of the initial location is consistent. See https://crbug.com/518740
  bounds.Offset(point_in_screen.x() - start_point_in_screen_.x(),
                point_in_screen.y() - start_point_in_screen_.y());
  return bounds;
}

void TabDragController::AdjustBrowserAndTabBoundsForDrag(
    int last_tabstrip_width,
    const gfx::Point& point_in_screen,
    std::vector<gfx::Rect>* drag_bounds) {
  attached_tabstrip_->InvalidateLayout();
  attached_tabstrip_->DoLayout();
  const int dragged_tabstrip_width = attached_tabstrip_->GetTabAreaWidth();

  // If the new tabstrip is smaller than the old resize the tabs.
  if (dragged_tabstrip_width < last_tabstrip_width) {
    const float leading_ratio =
        drag_bounds->front().x() / static_cast<float>(last_tabstrip_width);
    *drag_bounds = CalculateBoundsForDraggedTabs();

    if (drag_bounds->back().right() < dragged_tabstrip_width) {
      const int delta_x =
          std::min(static_cast<int>(leading_ratio * dragged_tabstrip_width),
                   dragged_tabstrip_width -
                       (drag_bounds->back().right() -
                        drag_bounds->front().x()));
      OffsetX(delta_x, drag_bounds);
    }

    // Reposition the restored window such that the tab that was dragged remains
    // under the mouse cursor.
    gfx::Rect tab_bounds = (*drag_bounds)[source_tab_index_];
    gfx::Point offset(
        gfx::ToRoundedInt(tab_bounds.width() * offset_to_width_ratio_) +
            tab_bounds.x(),
        0);
    views::View::ConvertPointToWidget(attached_tabstrip_, &offset);
    gfx::Rect bounds = GetAttachedBrowserWidget()->GetWindowBoundsInScreen();
    bounds.set_x(point_in_screen.x() - offset.x());
    GetAttachedBrowserWidget()->SetBounds(bounds);
  }
  attached_tabstrip_->SetTabBoundsForDrag(*drag_bounds);
}

Browser* TabDragController::CreateBrowserForDrag(
    TabStrip* source,
    const gfx::Point& point_in_screen,
    gfx::Vector2d* drag_offset,
    std::vector<gfx::Rect>* drag_bounds) {
  gfx::Rect new_bounds(CalculateDraggedBrowserBounds(source,
                                                     point_in_screen,
                                                     drag_bounds));
  *drag_offset = point_in_screen - new_bounds.origin();

  Profile* profile =
      Profile::FromBrowserContext(drag_data_[0].contents->GetBrowserContext());
  Browser::CreateParams create_params(Browser::TYPE_TABBED, profile, true);
  create_params.initial_bounds = new_bounds;
  Browser* browser = new Browser(create_params);
  is_dragging_new_browser_ = true;
  // If the window is created maximized then the bounds we supplied are ignored.
  // We need to reset them again so they are honored.
  browser->window()->SetBounds(new_bounds);

  return browser;
}

gfx::Point TabDragController::GetCursorScreenPoint() {
#if defined(OS_CHROMEOS)
  if (event_source_ == EVENT_SOURCE_TOUCH && env_->is_touch_down()) {
    views::Widget* widget = GetAttachedBrowserWidget();
    DCHECK(widget);
    aura::Window* widget_window = widget->GetNativeWindow();
    DCHECK(widget_window->GetRootWindow());
    gfx::PointF touch_point_f;
    bool got_touch_point =
        widget->GetGestureRecognizer()->GetLastTouchPointForTarget(
            widget_window, &touch_point_f);
    CHECK(got_touch_point);
    gfx::Point touch_point = gfx::ToFlooredPoint(touch_point_f);
    wm::ConvertPointToScreen(widget_window->GetRootWindow(), &touch_point);
    return touch_point;
  }
#endif

  return display::Screen::GetScreen()->GetCursorScreenPoint();
}

gfx::Vector2d TabDragController::GetWindowOffset(
    const gfx::Point& point_in_screen) {
  TabStrip* owning_tabstrip =
      attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
  views::View* toplevel_view = owning_tabstrip->GetWidget()->GetContentsView();

  gfx::Point point = point_in_screen;
  views::View::ConvertPointFromScreen(toplevel_view, &point);
  return point.OffsetFromOrigin();
}

TabDragController::Liveness TabDragController::GetLocalProcessWindow(
    const gfx::Point& screen_point,
    bool exclude_dragged_view,
    gfx::NativeWindow* window) {
  std::set<gfx::NativeWindow> exclude;
  if (exclude_dragged_view) {
    gfx::NativeWindow dragged_window =
        attached_tabstrip_->GetWidget()->GetNativeWindow();
    if (dragged_window)
      exclude.insert(dragged_window);
  }
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Exclude windows which are pending deletion via Browser::TabStripEmpty().
  // These windows can be returned in the Linux Aura port because the browser
  // window which was used for dragging is not hidden once all of its tabs are
  // attached to another browser window in DragBrowserToNewTabStrip().
  // TODO(pkotwicz): Fix this properly (crbug.com/358482)
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model()->empty())
      exclude.insert(browser->window()->GetNativeWindow());
  }
#endif
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  *window = window_finder_->GetLocalProcessWindowAtPoint(screen_point, exclude);
  return ref ? Liveness::ALIVE : Liveness::DELETED;
}

void TabDragController::SetTabDraggingInfo() {
#if defined(OS_CHROMEOS)
  TabStrip* dragged_tabstrip =
      attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
  DCHECK(dragged_tabstrip->IsDragSessionActive() && active_);

  aura::Window* dragged_window =
      GetWindowForTabDraggingProperties(dragged_tabstrip);
  aura::Window* source_window =
      GetWindowForTabDraggingProperties(source_tabstrip_);
  dragged_window->SetProperty(ash::kIsDraggingTabsKey, true);
  if (source_window != dragged_window) {
    dragged_window->SetProperty(ash::kTabDraggingSourceWindowKey,
                                source_window);
  }
#endif
}

void TabDragController::ClearTabDraggingInfo() {
#if defined(OS_CHROMEOS)
  TabStrip* dragged_tabstrip =
      attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
  DCHECK(!dragged_tabstrip->IsDragSessionActive() || !active_);
  // Do not clear the dragging info properties for a to-be-destroyed window.
  // They will be cleared later in Window's destructor. It's intentional as
  // ash::SplitViewController::TabDraggedWindowObserver listens to both
  // OnWindowDestroying() event and the window properties change event, and uses
  // the two events to decide what to do next.
  if (GetModel(dragged_tabstrip)->empty())
    return;

  aura::Window* dragged_window =
      GetWindowForTabDraggingProperties(dragged_tabstrip);
  dragged_window->ClearProperty(ash::kIsDraggingTabsKey);
  dragged_window->ClearProperty(ash::kTabDraggingSourceWindowKey);
  dragged_window->ClearProperty(ash::kTabDroppedWindowStateTypeKey);
#endif
}

void TabDragController::SetDeferredTargetTabstrip(
    TabStrip* deferred_target_tabstrip) {
#if defined(OS_CHROMEOS)
  if (!deferred_target_tabstrip_observer_) {
    deferred_target_tabstrip_observer_ =
        std::make_unique<DeferredTargetTabstripObserver>();
  }
  deferred_target_tabstrip_observer_->SetDeferredTargetTabstrip(
      deferred_target_tabstrip);
#endif
}
