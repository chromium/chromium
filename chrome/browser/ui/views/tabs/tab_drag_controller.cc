// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/i18n/rtl.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/root_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"  // nogncheck
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"  // nogncheck
#include "ui/aura/window_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"                            // nogncheck
#include "ui/aura/window.h"                         // nogncheck
#include "ui/wm/core/window_modality_controller.h"  // nogncheck
#endif

using content::OpenURLParams;
using content::WebContents;

// If non-null there is a drag underway.
static TabDragController* g_tab_drag_controller = nullptr;

namespace {

// Initial delay before moving tabs when the dragged tab is close to the edge of
// the stacked tabs.
constexpr auto kMoveAttachedInitialDelay =
    base::TimeDelta::FromMilliseconds(600);

// Delay for moving tabs after the initial delay has passed.
constexpr auto kMoveAttachedSubsequentDelay =
    base::TimeDelta::FromMilliseconds(300);

// A dragged window is forced to be a bit smaller than maximized bounds during a
// drag. This prevents the dragged browser widget from getting maximized at
// creation and makes it easier to drag tabs out of a restored window that had
// maximized size.
constexpr int kMaximizedWindowInset = 10;  // DIPs.

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Returns the aura::Window which stores the window properties for tab-dragging.
aura::Window* GetWindowForTabDraggingProperties(const TabDragContext* context) {
  return context ? context->AsView()->GetWidget()->GetNativeWindow() : nullptr;
}

// Returns true if |context| browser window is snapped.
bool IsSnapped(const TabDragContext* context) {
  DCHECK(context);
  chromeos::WindowStateType type =
      GetWindowForTabDraggingProperties(context)->GetProperty(
          chromeos::kWindowStateTypeKey);
  return type == chromeos::WindowStateType::kLeftSnapped ||
         type == chromeos::WindowStateType::kRightSnapped;
}

// In Chrome OS tablet mode, when dragging a tab/tabs around, the desired
// browser size during dragging is one-fourth of the workspace size or the
// window's minimum size.
gfx::Rect GetDraggedBrowserBoundsInTabletMode(aura::Window* window) {
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  gfx::Size mininum_size;
  if (window->delegate())
    mininum_size = window->delegate()->GetMinimumSize();

  gfx::Rect bounds(window->GetBoundsInScreen());
  bounds.set_width(std::max(work_area.width() / 2, mininum_size.width()));
  bounds.set_height(std::max(work_area.height() / 2, mininum_size.height()));
  return bounds;
}

// Store the current window bounds if we're in Chrome OS tablet mode and tab
// dragging is allowed on browser windows.
void StoreCurrentDraggedBrowserBoundsInTabletMode(
    aura::Window* window,
    const gfx::Rect& bounds_in_screen) {
  if (ash::TabletMode::Get()->InTabletMode()) {
    // The bounds that is stored in ash::kRestoreBoundsOverrideKey will be used
    // by DragDetails to calculate the window bounds during dragging in tablet
    // mode.
    window->SetProperty(ash::kRestoreBoundsOverrideKey,
                        new gfx::Rect(bounds_in_screen));
  }
}

// Returns true if |context| is currently showing in overview mode in Chrome
// OS.
bool IsShowingInOverview(TabDragContext* context) {
  return context && GetWindowForTabDraggingProperties(context)->GetProperty(
                        chromeos::kIsShowingInOverviewKey);
}

// Returns true if we should attach the dragged tabs into |target_context|
// after the drag ends. Currently it only happens on Chrome OS, when the dragged
// tabs are dragged over an overview window, we should not try to attach it
// to the overview window during dragging, but should wait to do so until the
// drag ends.
bool ShouldAttachOnEnd(TabDragContext* target_context) {
  return IsShowingInOverview(target_context);
}

// Returns true if |context| can detach from the current context and attach
// into another eligible browser window's context.
bool CanDetachFromTabStrip(TabDragContext* context) {
  return context && GetWindowForTabDraggingProperties(context)->GetProperty(
                        ash::kCanAttachToAnotherWindowKey);
}

#else
bool IsSnapped(const TabDragContext* context) {
  return false;
}

bool IsShowingInOverview(TabDragContext* context) {
  return false;
}

bool ShouldAttachOnEnd(TabDragContext* target_context) {
  return false;
}

bool CanDetachFromTabStrip(TabDragContext* context) {
  return true;
}

#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

void SetCapture(TabDragContext* context) {
  context->AsView()->GetWidget()->SetCapture(context->AsView());
}

gfx::Rect GetTabstripScreenBounds(const TabDragContext* context) {
  const views::View* view = context->AsView();
  gfx::Point view_topleft;
  views::View::ConvertPointToScreen(view, &view_topleft);
  gfx::Rect view_screen_bounds = view->GetLocalBounds();
  view_screen_bounds.Offset(view_topleft.x(), view_topleft.y());
  return view_screen_bounds;
}

// Returns true if |bounds| contains the y-coordinate |y|. The y-coordinate
// of |bounds| is adjusted by |vertical_adjustment|.
bool DoesRectContainVerticalPointExpanded(const gfx::Rect& bounds,
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

// KeyEventTracker installs an event monitor and runs a callback to end the drag
// when it receives any key event.
class KeyEventTracker : public ui::EventObserver {
 public:
  KeyEventTracker(base::OnceClosure end_drag_callback,
                  base::OnceClosure revert_drag_callback,
                  gfx::NativeWindow context)
      : end_drag_callback_(std::move(end_drag_callback)),
        revert_drag_callback_(std::move(revert_drag_callback)) {
    event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
        this, context, {ui::ET_KEY_PRESSED});
  }
  KeyEventTracker(const KeyEventTracker&) = delete;
  KeyEventTracker& operator=(const KeyEventTracker&) = delete;
  ~KeyEventTracker() override = default;

 private:
  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    if (event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE &&
        revert_drag_callback_) {
      std::move(revert_drag_callback_).Run();
    } else if (event.AsKeyEvent()->key_code() != ui::VKEY_ESCAPE &&
               end_drag_callback_) {
      std::move(end_drag_callback_).Run();
    }
  }

  base::OnceClosure end_drag_callback_;
  base::OnceClosure revert_drag_callback_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

class TabDragController::SourceTabStripEmptinessTracker
    : public TabStripModelObserver {
 public:
  explicit SourceTabStripEmptinessTracker(TabStripModel* tabstrip,
                                          TabDragController* parent)
      : tab_strip_(tabstrip), parent_(parent) {
    tab_strip_->AddObserver(this);
  }

 private:
  void TabStripEmpty() override {
    tab_strip_->RemoveObserver(this);
    parent_->OnSourceTabStripEmpty();
  }

  TabStripModel* const tab_strip_;
  TabDragController* const parent_;
};

class TabDragController::DraggedTabsClosedTracker
    : public TabStripModelObserver {
 public:
  DraggedTabsClosedTracker(TabStripModel* tabstrip, TabDragController* parent)
      : parent_(parent) {
    tabstrip->AddObserver(this);
  }

  void OnTabStripModelChanged(
      TabStripModel* model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    switch (change.type()) {
      case TabStripModelChange::Type::kRemoved:
        for (const auto& contents : change.GetRemove()->contents)
          parent_->OnActiveStripWebContentsRemoved(contents.contents);
        break;
      case TabStripModelChange::Type::kReplaced:
        parent_->OnActiveStripWebContentsReplaced(
            change.GetReplace()->old_contents,
            change.GetReplace()->new_contents);
        break;
      default:
        break;
    }
  }

 private:
  TabDragController* const parent_;
};

TabDragController::TabDragData::TabDragData()
    : contents(nullptr),
      source_model_index(TabStripModel::kNoTab),
      attached_view(nullptr),
      pinned(false) {}

TabDragController::TabDragData::~TabDragData() {}

TabDragController::TabDragData::TabDragData(TabDragData&&) = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)

// The class to track the current deferred target tabstrip and also to observe
// its native window's property ash::kIsDeferredTabDraggingTargetWindowKey.
// The reason we need to observe the window property is the property might be
// cleared outside of TabDragController (i.e. by ash), and we should update the
// tracked deferred target tabstrip in this case.
class TabDragController::DeferredTargetTabstripObserver
    : public aura::WindowObserver {
 public:
  DeferredTargetTabstripObserver() = default;
  DeferredTargetTabstripObserver(const DeferredTargetTabstripObserver&) =
      delete;
  DeferredTargetTabstripObserver& operator=(
      const DeferredTargetTabstripObserver&) = delete;
  ~DeferredTargetTabstripObserver() override {
    if (deferred_target_context_) {
      GetWindowForTabDraggingProperties(deferred_target_context_)
          ->RemoveObserver(this);
      deferred_target_context_ = nullptr;
    }
  }

  void SetDeferredTargetTabstrip(TabDragContext* deferred_target_context) {
    if (deferred_target_context_ == deferred_target_context)
      return;

    // Clear the window property on the previous |deferred_target_context_|.
    if (deferred_target_context_) {
      aura::Window* old_window =
          GetWindowForTabDraggingProperties(deferred_target_context_);
      old_window->RemoveObserver(this);
      old_window->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);
    }

    deferred_target_context_ = deferred_target_context;

    // Set the window property on the new |deferred_target_context_|.
    if (deferred_target_context_) {
      aura::Window* new_window =
          GetWindowForTabDraggingProperties(deferred_target_context_);
      new_window->SetProperty(ash::kIsDeferredTabDraggingTargetWindowKey, true);
      new_window->AddObserver(this);
    }
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window,
              GetWindowForTabDraggingProperties(deferred_target_context_));

    if (key == ash::kIsDeferredTabDraggingTargetWindowKey &&
        !window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
      SetDeferredTargetTabstrip(nullptr);
    }

    // else do nothing. currently it's only possible that ash clears the window
    // property, but doesn't set the window property.
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window,
              GetWindowForTabDraggingProperties(deferred_target_context_));
    SetDeferredTargetTabstrip(nullptr);
  }

  TabDragContext* deferred_target_context() { return deferred_target_context_; }

 private:
  TabDragContext* deferred_target_context_ = nullptr;
};

#endif

///////////////////////////////////////////////////////////////////////////////
// TabDragController, public:

// static
const int TabDragController::kTouchVerticalDetachMagnetism = 50;

// static
const int TabDragController::kVerticalDetachMagnetism = 15;

TabDragController::TabDragController()
    : current_state_(DragState::kNotStarted),
      event_source_(EVENT_SOURCE_MOUSE),
      source_context_(nullptr),
      attached_context_(nullptr),
      can_release_capture_(true),
      offset_to_width_ratio_(0),
      old_focused_view_tracker_(std::make_unique<views::ViewTracker>()),
      last_move_screen_loc_(0),
      source_view_index_(std::numeric_limits<size_t>::max()),
      initial_move_(true),
      detach_behavior_(DETACHABLE),
      move_behavior_(REORDER),
      mouse_has_ever_moved_left_(false),
      mouse_has_ever_moved_right_(false),
      is_dragging_new_browser_(false),
      was_source_maximized_(false),
      was_source_fullscreen_(false),
      did_restore_window_(false),
      tab_strip_to_attach_to_after_exit_(nullptr),
      move_loop_widget_(nullptr),
      is_mutating_(false),
      attach_x_(-1),
      attach_index_(-1) {
  g_tab_drag_controller = this;
}

TabDragController::~TabDragController() {
  if (g_tab_drag_controller == this)
    g_tab_drag_controller = nullptr;

  widget_observation_.Reset();

  if (is_dragging_window())
    GetAttachedBrowserWidget()->EndMoveLoop();

  if (event_source_ == EVENT_SOURCE_TOUCH) {
    TabDragContext* capture_context =
        attached_context_ ? attached_context_ : source_context_;
    capture_context->AsView()->GetWidget()->ReleaseCapture();
  }
  CHECK(!IsInObserverList());
}

void TabDragController::Init(TabDragContext* source_context,
                             TabSlotView* source_view,
                             const std::vector<TabSlotView*>& dragging_views,
                             const gfx::Point& mouse_offset,
                             int source_view_offset,
                             ui::ListSelectionModel initial_selection_model,
                             MoveBehavior move_behavior,
                             EventSource event_source) {
  DCHECK(!dragging_views.empty());
  DCHECK(base::Contains(dragging_views, source_view));
  source_context_ = source_context;
  was_source_maximized_ = source_context->AsView()->GetWidget()->IsMaximized();
  was_source_fullscreen_ =
      source_context->AsView()->GetWidget()->IsFullscreen();
  // Do not release capture when transferring capture between widgets on:
  // - Desktop Linux
  //     Mouse capture is not synchronous on desktop Linux. Chrome makes
  //     transferring capture between widgets without releasing capture appear
  //     synchronous on desktop Linux, so use that.
  // - Chrome OS
  //     Releasing capture on Ash cancels gestures so avoid it.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  can_release_capture_ = false;
#endif
  start_point_in_screen_ = gfx::Point(source_view_offset, mouse_offset.y());
  views::View::ConvertPointToScreen(source_view, &start_point_in_screen_);
  event_source_ = event_source;
  mouse_offset_ = mouse_offset;
  move_behavior_ = move_behavior;
  last_point_in_screen_ = start_point_in_screen_;
  last_move_screen_loc_ = start_point_in_screen_.x();
  initial_tab_positions_ = source_context->GetTabXCoordinates();

  source_context_emptiness_tracker_ =
      std::make_unique<SourceTabStripEmptinessTracker>(
          source_context_->GetTabStripModel(), this);

  header_drag_ = source_view->GetTabSlotViewType() ==
                 TabSlotView::ViewType::kTabGroupHeader;
  if (header_drag_)
    group_ = source_view->group();

  drag_data_.resize(dragging_views.size());
  for (size_t i = 0; i < dragging_views.size(); ++i)
    InitDragData(dragging_views[i], &(drag_data_[i]));
  source_view_index_ =
      std::find(dragging_views.begin(), dragging_views.end(), source_view) -
      dragging_views.begin();

  // Listen for Esc key presses.
  key_event_tracker_ = std::make_unique<KeyEventTracker>(
      base::BindOnce(&TabDragController::EndDrag, base::Unretained(this),
                     END_DRAG_COMPLETE),
      base::BindOnce(&TabDragController::EndDrag, base::Unretained(this),
                     END_DRAG_CANCEL),
      source_context_->AsView()->GetWidget()->GetNativeWindow());

  if (source_view->width() > 0) {
    offset_to_width_ratio_ =
        float{source_view->GetMirroredXInView(source_view_offset)} /
        float{source_view->width()};
  }
  InitWindowCreatePoint();
  initial_selection_model_ = std::move(initial_selection_model);

  // Gestures don't automatically do a capture. We don't allow multiple drags at
  // the same time, so we explicitly capture.
  if (event_source == EVENT_SOURCE_TOUCH) {
    // Taking capture may cause capture to be lost, ending the drag and
    // destroying |this|.
    base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
    SetCapture(source_context_);
    if (!ref)
      return;
  }

  window_finder_ = std::make_unique<WindowFinder>();
}

// static
bool TabDragController::IsAttachedTo(const TabDragContext* context) {
  return (g_tab_drag_controller && g_tab_drag_controller->active() &&
          g_tab_drag_controller->attached_context() == context);
}

// static
bool TabDragController::IsActive() {
  return g_tab_drag_controller && g_tab_drag_controller->active();
}

// static
TabDragContext* TabDragController::GetSourceContext() {
  return g_tab_drag_controller ? g_tab_drag_controller->source_context_
                               : nullptr;
}

void TabDragController::SetMoveBehavior(MoveBehavior behavior) {
  if (current_state_ == DragState::kNotStarted)
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
  TRACE_EVENT1("views", "TabDragController::Drag", "point_in_screen",
               point_in_screen.ToString());

  bring_to_front_timer_.Stop();
  move_stacked_timer_.Stop();

  if (current_state_ == DragState::kWaitingToDragTabs ||
      current_state_ == DragState::kWaitingToStop ||
      current_state_ == DragState::kStopped)
    return;

  if (current_state_ == DragState::kNotStarted) {
    if (!CanStartDrag(point_in_screen))
      return;  // User hasn't dragged far enough yet.

    // On windows SaveFocus() may trigger a capture lost, which destroys us.
    {
      base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
      SaveFocus();
      if (!ref)
        return;
    }
    current_state_ = DragState::kDraggingTabs;
    Attach(source_context_, gfx::Point());
    if (num_dragging_tabs() == source_context_->GetTabStripModel()->count()) {
      views::Widget* widget = GetAttachedBrowserWidget();
      gfx::Rect new_bounds;
      gfx::Vector2d drag_offset;
      if (was_source_maximized_ || was_source_fullscreen_) {
        did_restore_window_ = true;
        // When all tabs in a maximized browser are dragged the browser gets
        // restored during the drag and maximized back when the drag ends.
        const int tab_area_width = attached_context_->GetTabDragAreaWidth();
        std::vector<gfx::Rect> drag_bounds =
            attached_context_->CalculateBoundsForDraggedViews(attached_views_);
        OffsetX(GetAttachedDragPoint(point_in_screen).x(), &drag_bounds);
        new_bounds = CalculateDraggedBrowserBounds(
            source_context_, point_in_screen, &drag_bounds);
        new_bounds.Offset(-widget->GetRestoredBounds().x() +
                              point_in_screen.x() - mouse_offset_.x(),
                          0);
        widget->SetVisibilityChangedAnimationsEnabled(false);
        widget->Restore();
        widget->SetBounds(new_bounds);
        drag_offset = GetWindowOffset(point_in_screen);
        AdjustBrowserAndTabBoundsForDrag(tab_area_width, point_in_screen,
                                         &drag_offset, &drag_bounds);
        widget->SetVisibilityChangedAnimationsEnabled(true);
      } else {
        new_bounds =
            CalculateNonMaximizedDraggedBrowserBounds(widget, point_in_screen);
        widget->SetBounds(new_bounds);
        drag_offset = GetWindowOffset(point_in_screen);
      }

#if BUILDFLAG(IS_CHROMEOS_ASH)
      StoreCurrentDraggedBrowserBoundsInTabletMode(widget->GetNativeWindow(),
                                                   new_bounds);
#endif

      RunMoveLoop(drag_offset);
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
  if (reason == END_DRAG_CAPTURE_LOST &&
      current_state_ == DragState::kDraggingWindow) {
    return;
  }

  // If we're dragging a window, end the move loop, returning control to
  // RunMoveLoop() which will end the drag.
  if (current_state_ == DragState::kDraggingWindow) {
    current_state_ = DragState::kWaitingToStop;
    GetAttachedBrowserWidget()->EndMoveLoop();
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // It's possible that in Chrome OS we defer the windows that are showing in
  // overview to attach into during dragging. If so we need to attach the
  // dragged tabs to it first.
  if (reason == END_DRAG_COMPLETE && deferred_target_context_observer_)
    PerformDeferredAttach();

  // It's also possible that we need to merge the dragged tabs back into the
  // source window even if the dragged tabs is dragged away from the source
  // window.
  if (source_context_ &&
      GetWindowForTabDraggingProperties(source_context_)
          ->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
    GetWindowForTabDraggingProperties(source_context_)
        ->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);
    reason = END_DRAG_CANCEL;
  }
#endif

  EndDragImpl(reason != END_DRAG_COMPLETE && source_context_ ? CANCELED
                                                             : NORMAL);
}

void TabDragController::InitDragData(TabSlotView* view,
                                     TabDragData* drag_data) {
  TRACE_EVENT0("views", "TabDragController::InitDragData");
  const int source_model_index = source_context_->GetIndexOf(view);
  drag_data->source_model_index = source_model_index;
  if (source_model_index != TabStripModel::kNoTab) {
    drag_data->contents = source_context_->GetTabStripModel()->GetWebContentsAt(
        drag_data->source_model_index);
    drag_data->pinned = source_context_->IsTabPinned(static_cast<Tab*>(view));
  }
  base::Optional<tab_groups::TabGroupId> tab_group_id = view->group();
  if (tab_group_id.has_value()) {
    drag_data->tab_group_data = TabDragData::TabGroupData{
        tab_group_id.value(), *source_context_->GetTabStripModel()
                                   ->group_model()
                                   ->GetTabGroup(tab_group_id.value())
                                   ->visual_data()};
  }
}

void TabDragController::OnWidgetBoundsChanged(views::Widget* widget,
                                              const gfx::Rect& new_bounds) {
  TRACE_EVENT1("views", "TabDragController::OnWidgetBoundsChanged",
               "new_bounds", new_bounds.ToString());
  // Detaching and attaching can be suppresed temporarily to suppress attaching
  // to incorrect window on changing bounds. We should prevent Drag() itself,
  // otherwise it can clear deferred attaching tab.
  if (!CanDetachFromTabStrip(attached_context_))
    return;
#if defined(USE_AURA)
  aura::Env* env = aura::Env::GetInstance();
  // WidgetBoundsChanged happens as a step of ending a drag, but Drag() doesn't
  // have to be called -- GetCursorScreenPoint() may return an incorrect
  // location in such case and causes a weird effect. See
  // https://crbug.com/914527 for the details.
  if (!env->IsMouseButtonDown() && !env->is_touch_down())
    return;
#endif
  Drag(GetCursorScreenPoint());
}

void TabDragController::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
}

void TabDragController::OnSourceTabStripEmpty() {
  // NULL out source_context_ so that we don't attempt to add back to it (in
  // the case of a revert).
  source_context_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Also update the source window info for the current dragged window.
  if (attached_context_) {
    GetWindowForTabDraggingProperties(attached_context_)
        ->ClearProperty(ash::kTabDraggingSourceWindowKey);
  }
#endif
}

void TabDragController::OnActiveStripWebContentsRemoved(
    content::WebContents* contents) {
  // Mark closed tabs as destroyed so we don't try to manipulate them later.
  for (auto& drag_datum : drag_data_) {
    if (drag_datum.contents == contents) {
      drag_datum.contents = nullptr;
      break;
    }
  }
}

void TabDragController::OnActiveStripWebContentsReplaced(
    content::WebContents* previous,
    content::WebContents* next) {
  for (auto& drag_datum : drag_data_) {
    if (drag_datum.contents == previous) {
      drag_datum.contents = next;
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, private:

void TabDragController::InitWindowCreatePoint() {
  // window_create_point_ is only used in CompleteDrag() (through
  // GetWindowCreatePoint() to get the start point of the docked window) when
  // the attached_context_ is NULL and all the window's related bound
  // information are obtained from source_context_. So, we need to get the
  // first_tab based on source_context_, not attached_context_. Otherwise,
  // the window_create_point_ is not in the correct coordinate system. Please
  // refer to http://crbug.com/6223 comment #15 for detailed information.
  views::View* first_tab = source_context_->GetTabAt(0);
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
  DCHECK(source_context_);
  old_focused_view_tracker_->SetView(
      source_context_->AsView()->GetFocusManager()->GetFocusedView());
  source_context_->AsView()->GetFocusManager()->ClearFocus();
  // WARNING: we may have been deleted.
}

void TabDragController::RestoreFocus() {
  if (attached_context_ != source_context_) {
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
  return sqrt(pow(float{x_offset}, 2) + pow(float{y_offset}, 2)) >
         kMinimumDragDistance;
}

TabDragController::Liveness TabDragController::ContinueDragging(
    const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::ContinueDragging",
               "point_in_screen", point_in_screen.ToString());

  DCHECK(attached_context_);

  TabDragContext* target_context = source_context_;
  if (detach_behavior_ == DETACHABLE &&
      GetTargetTabStripForPoint(point_in_screen, &target_context) ==
          Liveness::DELETED) {
    return Liveness::DELETED;
  }

  // The dragged tabs may not be able to attach into |target_context| during
  // dragging if the window accociated with |target_context| is currently
  // showing in overview mode in Chrome OS, in this case we defer attaching into
  // it till the drag ends and reset |target_context| here.
  if (ShouldAttachOnEnd(target_context)) {
    SetDeferredTargetTabstrip(target_context);
    target_context = current_state_ == DragState::kDraggingWindow
                         ? attached_context_
                         : nullptr;
  } else {
    SetDeferredTargetTabstrip(nullptr);
  }

  bool tab_strip_changed = (target_context != attached_context_);

  if (attached_context_) {
    int move_delta = point_in_screen.x() - last_point_in_screen_.x();
    if (move_delta > 0)
      mouse_has_ever_moved_right_ = true;
    else if (move_delta < 0)
      mouse_has_ever_moved_left_ = true;
  }
  last_point_in_screen_ = point_in_screen;

  if (tab_strip_changed) {
    is_dragging_new_browser_ = false;
    did_restore_window_ = false;
    if (DragBrowserToNewTabStrip(target_context, point_in_screen) ==
        DRAG_BROWSER_RESULT_STOP) {
      return Liveness::ALIVE;
    }
  }
  if (current_state_ == DragState::kDraggingWindow) {
    bring_to_front_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(750),
        base::BindOnce(&TabDragController::BringWindowUnderPointToFront,
                       base::Unretained(this), point_in_screen));
  }

  if (current_state_ == DragState::kDraggingTabs) {
    if (move_only()) {
      DragActiveTabStacked(point_in_screen);
    } else {
      MoveAttached(point_in_screen, false);
      if (tab_strip_changed) {
        // Move the corresponding window to the front. We do this after the
        // move as on windows activate triggers a synchronous paint.
        attached_context_->AsView()->GetWidget()->Activate();
      }
    }
  }
  return Liveness::ALIVE;
}

TabDragController::DragBrowserResultType
TabDragController::DragBrowserToNewTabStrip(TabDragContext* target_context,
                                            const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::DragBrowserToNewTabStrip",
               "point_in_screen", point_in_screen.ToString());

  if (!target_context) {
    DetachIntoNewBrowserAndRunMoveLoop(point_in_screen);
    return DRAG_BROWSER_RESULT_STOP;
  }

#if defined(USE_AURA)
  // Only Aura windows are gesture consumers.
  gfx::NativeView attached_native_view =
      GetAttachedBrowserWidget()->GetNativeView();
  GetAttachedBrowserWidget()->GetGestureRecognizer()->TransferEventsTo(
      attached_native_view,
      target_context->AsView()->GetWidget()->GetNativeView(),
      ui::TransferTouchesBehavior::kDontCancel);
#endif

  if (current_state_ == DragState::kDraggingWindow) {
    // ReleaseCapture() is going to result in calling back to us (because it
    // results in a move). That'll cause all sorts of problems.  Reset the
    // observer so we don't get notified and process the event.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    widget_observation_.Reset();
    move_loop_widget_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    views::Widget* browser_widget = GetAttachedBrowserWidget();
    // Need to release the drag controller before starting the move loop as it's
    // going to trigger capture lost, which cancels drag.
    attached_context_->ReleaseDragController();
    target_context->OwnDragController(this);
    // Disable animations so that we don't see a close animation on aero.
    browser_widget->SetVisibilityChangedAnimationsEnabled(false);
    if (can_release_capture_)
      browser_widget->ReleaseCapture();
    else
      SetCapture(target_context);

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
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
      tab_strip_to_attach_to_after_exit_ = target_context;
      current_state_ = DragState::kWaitingToDragTabs;
    } else {
      Detach(DONT_RELEASE_CAPTURE);
      Attach(target_context, point_in_screen);
      current_state_ = DragState::kDraggingTabs;
      // Move the tabs into position.
      MoveAttached(point_in_screen, true);
      attached_context_->AsView()->GetWidget()->Activate();
    }

    return DRAG_BROWSER_RESULT_STOP;
  }
  Detach(DONT_RELEASE_CAPTURE);
  Attach(target_context, point_in_screen);
  MoveAttached(point_in_screen, true);
  return DRAG_BROWSER_RESULT_CONTINUE;
}

void TabDragController::DragActiveTabStacked(
    const gfx::Point& point_in_screen) {
  if (attached_context_->GetTabCount() != int{initial_tab_positions_.size()})
    return;  // TODO: should cancel drag if this happens.

  int delta = point_in_screen.x() - start_point_in_screen_.x();
  attached_context_->DragActiveTabStacked(initial_tab_positions_, delta);
}

void TabDragController::MoveAttachedToNextStackedIndex(
    const gfx::Point& point_in_screen) {
  int index = *attached_context_->GetActiveTouchIndex();
  if (index + 1 >= attached_context_->GetTabCount())
    return;

  attached_context_->GetTabStripModel()->MoveSelectedTabsTo(index + 1);
  StartMoveStackedTimerIfNecessary(point_in_screen,
                                   kMoveAttachedSubsequentDelay);
}

void TabDragController::MoveAttachedToPreviousStackedIndex(
    const gfx::Point& point_in_screen) {
  int index = *attached_context_->GetActiveTouchIndex();
  if (index <= attached_context_->GetPinnedTabCount())
    return;

  attached_context_->GetTabStripModel()->MoveSelectedTabsTo(index - 1);
  StartMoveStackedTimerIfNecessary(point_in_screen,
                                   kMoveAttachedSubsequentDelay);
}

void TabDragController::MoveAttached(const gfx::Point& point_in_screen,
                                     bool just_attached) {
  DCHECK(attached_context_);
  DCHECK_EQ(current_state_, DragState::kDraggingTabs);

  gfx::Point dragged_view_point = GetAttachedDragPoint(point_in_screen);

  const int threshold = attached_context_->GetHorizontalDragThreshold();

  std::vector<TabSlotView*> views(drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    views[i] = drag_data_[i].attached_view;

  bool did_layout = false;
  // Update the model, moving the WebContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter), or if this the first move and the tabs are not consecutive, or if
  // we have just attached to a new tabstrip and need to move to the correct
  // initial position.
  if (just_attached ||
      (abs(point_in_screen.x() - last_move_screen_loc_) > threshold) ||
      (initial_move_ && !AreTabsConsecutive())) {
    TabStripModel* attached_model = attached_context_->GetTabStripModel();
    int to_index = attached_context_->GetInsertionIndexForDraggedBounds(
        GetDraggedViewTabStripBounds(dragged_view_point),
        GetViewsMatchingDraggedContents(attached_context_), num_dragging_tabs(),
        mouse_has_ever_moved_left_, mouse_has_ever_moved_right_, group_);
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
      views::View::ConvertPointFromScreen(attached_context_->AsView(),
                                          &tab_strip_point);
      const int new_x =
          attached_context_->AsView()->GetMirroredXInView(tab_strip_point.x());
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
        // TabDragContext determines if the tabs needs to be animated
        // based on model position. This means we need to invoke
        // LayoutDraggedTabsAt before changing the model.
        attached_context_->LayoutDraggedViewsAt(
            views, source_view_drag_data()->attached_view, dragged_view_point,
            initial_move_);
        did_layout = true;
      }

      attached_model->MoveSelectedTabsTo(to_index);

      if (header_drag_) {
        attached_model->MoveTabGroup(group_.value());
      } else {
        UpdateGroupForDraggedTabs();
      }

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
    attached_context_->LayoutDraggedViewsAt(
        views, source_view_drag_data()->attached_view, dragged_view_point,
        initial_move_);
  }

  StartMoveStackedTimerIfNecessary(point_in_screen, kMoveAttachedInitialDelay);

  initial_move_ = false;
}

void TabDragController::StartMoveStackedTimerIfNecessary(
    const gfx::Point& point_in_screen,
    base::TimeDelta delay) {
  DCHECK(attached_context_);

  base::Optional<int> touch_index = attached_context_->GetActiveTouchIndex();
  if (!touch_index)
    return;

  gfx::Point dragged_view_point = GetAttachedDragPoint(point_in_screen);
  gfx::Rect bounds = GetDraggedViewTabStripBounds(dragged_view_point);
  if (attached_context_->ShouldDragToNextStackedTab(
          bounds, *touch_index, mouse_has_ever_moved_right_)) {
    move_stacked_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&TabDragController::MoveAttachedToNextStackedIndex,
                       base::Unretained(this), point_in_screen));
  } else if (attached_context_->ShouldDragToPreviousStackedTab(
                 bounds, *touch_index, mouse_has_ever_moved_left_)) {
    move_stacked_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&TabDragController::MoveAttachedToPreviousStackedIndex,
                       base::Unretained(this), point_in_screen));
  }
}

TabDragController::DetachPosition TabDragController::GetDetachPosition(
    const gfx::Point& point_in_screen) {
  DCHECK(attached_context_);
  gfx::Point attached_point(point_in_screen);
  views::View::ConvertPointFromScreen(attached_context_->AsView(),
                                      &attached_point);
  if (attached_point.x() < attached_context_->TabDragAreaBeginX())
    return DETACH_BEFORE;
  if (attached_point.x() >= attached_context_->TabDragAreaEndX())
    return DETACH_AFTER;
  return DETACH_ABOVE_OR_BELOW;
}

TabDragController::Liveness TabDragController::GetTargetTabStripForPoint(
    const gfx::Point& point_in_screen,
    TabDragContext** context) {
  *context = nullptr;
  TRACE_EVENT1("views", "TabDragController::GetTargetTabStripForPoint",
               "point_in_screen", point_in_screen.ToString());

  if (move_only() && attached_context_) {
    // move_only() is intended for touch, in which case we only want to detach
    // if the touch point moves significantly in the vertical distance.
    gfx::Rect tabstrip_bounds = GetTabstripScreenBounds(attached_context_);
    if (DoesRectContainVerticalPointExpanded(tabstrip_bounds,
                                             kTouchVerticalDetachMagnetism,
                                             point_in_screen.y())) {
      *context = attached_context_;
      return Liveness::ALIVE;
    }
  }
  gfx::NativeWindow local_window;
  const Liveness state = GetLocalProcessWindow(
      point_in_screen, current_state_ == DragState::kDraggingWindow,
      &local_window);
  if (state == Liveness::DELETED)
    return Liveness::DELETED;

  if (local_window && CanAttachTo(local_window)) {
    TabDragContext* destination_tab_strip =
        BrowserView::GetBrowserViewForNativeWindow(local_window)
            ->tabstrip()
            ->GetDragContext();
    if (ShouldAttachOnEnd(destination_tab_strip)) {
      // No need to check if the specified screen point is within the bounds of
      // the tabstrip as arriving here we know that the window is currently
      // showing in overview mode in Chrome OS and its bounds contain the
      // specified screen point, and these two conditions are enough for a
      // window to be a valid target window to attach the dragged tabs.
      *context = destination_tab_strip;
      return Liveness::ALIVE;
    } else if (destination_tab_strip &&
               DoesTabStripContain(destination_tab_strip, point_in_screen)) {
      *context = destination_tab_strip;
      return Liveness::ALIVE;
    }
  }

  *context = current_state_ == DragState::kDraggingWindow ? attached_context_
                                                          : nullptr;
  return Liveness::ALIVE;
}

bool TabDragController::DoesTabStripContain(
    TabDragContext* context,
    const gfx::Point& point_in_screen) const {
  // Make sure the specified screen point is actually within the bounds of the
  // specified context...
  gfx::Rect tabstrip_bounds = GetTabstripScreenBounds(context);
  const int x_in_strip = point_in_screen.x() - tabstrip_bounds.x();
  return (x_in_strip >= context->TabDragAreaBeginX()) &&
         (x_in_strip < context->TabDragAreaEndX()) &&
         DoesRectContainVerticalPointExpanded(
             tabstrip_bounds, kVerticalDetachMagnetism, point_in_screen.y());
}

void TabDragController::Attach(TabDragContext* attached_context,
                               const gfx::Point& point_in_screen,
                               bool set_capture) {
  TRACE_EVENT1("views", "TabDragController::Attach", "point_in_screen",
               point_in_screen.ToString());

  DCHECK(!attached_context_);  // We should already have detached by the time
                               // we get here.

  attached_context_ = attached_context;

  std::vector<TabSlotView*> views =
      GetViewsMatchingDraggedContents(attached_context_);

  if (views.empty()) {
    // Transitioning from detached to attached to a new context. Add tabs to
    // the new model.

    selection_model_before_attach_ =
        attached_context->GetTabStripModel()->selection_model();

    // Register a new group if necessary, so that the insertion index in the
    // tab strip can be calculated based on the group membership of tabs.
    if (header_drag_) {
      attached_context_->GetTabStripModel()->group_model()->AddTabGroup(
          group_.value(),
          source_view_drag_data()->tab_group_data.value().group_visual_data);
    }

    // Insert at any valid index in the tabstrip. We'll fix up the insertion
    // index in MoveAttached() later.
    int index = attached_context_->GetPinnedTabCount();
    attach_index_ = index;

    gfx::Point tab_strip_point(point_in_screen);
    views::View::ConvertPointFromScreen(attached_context_->AsView(),
                                        &tab_strip_point);
    tab_strip_point.set_x(
        attached_context_->AsView()->GetMirroredXInView(tab_strip_point.x()));
    tab_strip_point.Offset(0, -mouse_offset_.y());
    attach_x_ = tab_strip_point.x();

    base::AutoReset<bool> setter(&is_mutating_, true);
    for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
      int add_types = TabStripModel::ADD_NONE;
      if (attached_context_->GetActiveTouchIndex()) {
        // StackedTabStripLayout positions relative to the active tab, if we
        // don't add the tab as active things bounce around.
        DCHECK_EQ(1u, drag_data_.size());
        add_types |= TabStripModel::ADD_ACTIVE;
      }
      if (drag_data_[i].pinned)
        add_types |= TabStripModel::ADD_PINNED;

      // We should have owned_contents here, this CHECK is used to gather data
      // for https://crbug.com/677806.
      CHECK(drag_data_[i].owned_contents);
      attached_context_->GetTabStripModel()->InsertWebContentsAt(
          index + i - first_tab_index(),
          std::move(drag_data_[i].owned_contents), add_types, group_);

      // If a sad tab is showing, the SadTabView needs to be updated.
      SadTabHelper* sad_tab_helper =
          SadTabHelper::FromWebContents(drag_data_[i].contents);
      if (sad_tab_helper)
        sad_tab_helper->ReinstallInWebView();
    }

    views = GetViewsMatchingDraggedContents(attached_context_);
  }
  DCHECK_EQ(views.size(), drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    drag_data_[i].attached_view = views[i];
    attached_views_.push_back(views[i]);
  }

  ResetSelection(attached_context_->GetTabStripModel());

  // This should be called after ResetSelection() in order to generate
  // bounds correctly. http://crbug.com/836004
  attached_context_->StartedDragging(views);

  // The size of the dragged tab may have changed. Adjust the x offset so that
  // ratio of mouse_offset_ to original width is maintained.
  std::vector<TabSlotView*> tabs_to_source(views);
  tabs_to_source.erase(tabs_to_source.begin() + source_view_index_ + 1,
                       tabs_to_source.end());
  int new_x = TabStrip::GetSizeNeededForViews(tabs_to_source) -
              views[source_view_index_]->width() +
              base::ClampRound(offset_to_width_ratio_ *
                               views[source_view_index_]->width());
  mouse_offset_.set_x(new_x);

  // Transfer ownership of us to the new tabstrip as well as making sure the
  // window has capture. This is important so that if activation changes the
  // drag isn't prematurely canceled.
  if (set_capture)
    SetCapture(attached_context_);
  attached_context_->OwnDragController(this);
  SetTabDraggingInfo();
  attached_context_tabs_closed_tracker_ =
      std::make_unique<DraggedTabsClosedTracker>(
          attached_context_->GetTabStripModel(), this);

  if (attach_index_ != -1 && !header_drag_)
    UpdateGroupForDraggedTabs();
}

void TabDragController::Detach(ReleaseCapture release_capture) {
  TRACE_EVENT1("views", "TabDragController::Detach", "release_capture",
               release_capture);

  attached_context_tabs_closed_tracker_.reset();

  attach_index_ = -1;

  // When the user detaches we assume they want to reorder.
  move_behavior_ = REORDER;

  // Release ownership of the drag controller and mouse capture. When we
  // reattach ownership is transfered.
  attached_context_->ReleaseDragController();
  if (release_capture == RELEASE_CAPTURE)
    attached_context_->AsView()->GetWidget()->ReleaseCapture();

  mouse_has_ever_moved_left_ = true;
  mouse_has_ever_moved_right_ = true;

  TabStripModel* attached_model = attached_context_->GetTabStripModel();

  std::vector<TabRendererData> tab_data;
  for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
    tab_data.push_back(static_cast<Tab*>(drag_data_[i].attached_view)->data());
    int index = attached_model->GetIndexOfWebContents(drag_data_[i].contents);
    DCHECK_NE(-1, index);

    // Hide the tab so that the user doesn't see it animate closed.
    drag_data_[i].attached_view->SetVisible(false);
    drag_data_[i].attached_view->set_detached();
    drag_data_[i].owned_contents = attached_model->DetachWebContentsAt(index);

    // Detaching may end up deleting the tab, drop references to it.
    drag_data_[i].attached_view = nullptr;
  }
  if (header_drag_)
    source_view_drag_data()->attached_view = nullptr;

  // If we've removed the last Tab from the TabDragContext, hide the
  // frame now.
  if (!attached_model->empty()) {
    if (!selection_model_before_attach_.empty() &&
        selection_model_before_attach_.active() >= 0 &&
        selection_model_before_attach_.active() < attached_model->count()) {
      // Restore the selection.
      attached_model->SetSelectionFromModel(selection_model_before_attach_);
    } else if (attached_context_ == source_context_ &&
               !initial_selection_model_.empty()) {
      RestoreInitialSelection();
    }
  }

  ClearTabDraggingInfo();
  attached_context_->DraggedTabsDetached();
  attached_context_ = nullptr;
  attached_views_.clear();
}

void TabDragController::DetachIntoNewBrowserAndRunMoveLoop(
    const gfx::Point& point_in_screen) {
  if (attached_context_->GetTabStripModel()->count() == num_dragging_tabs()) {
    // All the tabs in a browser are being dragged but all the tabs weren't
    // initially being dragged. For this to happen the user would have to
    // start dragging a set of tabs, the other tabs close, then detach.
    RunMoveLoop(GetWindowOffset(point_in_screen));
    return;
  }

  const int tab_area_width = attached_context_->GetTabDragAreaWidth();
  std::vector<gfx::Rect> drag_bounds =
      attached_context_->CalculateBoundsForDraggedViews(attached_views_);
  OffsetX(GetAttachedDragPoint(point_in_screen).x(), &drag_bounds);

  gfx::Vector2d drag_offset;
  Browser* browser = CreateBrowserForDrag(attached_context_, point_in_screen,
                                          &drag_offset, &drag_bounds);

  BrowserView* dragged_browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::Widget* dragged_widget = dragged_browser_view->GetWidget();

#if defined(USE_AURA)
  // Only Aura windows are gesture consumers.
  views::Widget* attached_widget = attached_context_->AsView()->GetWidget();
  // Unlike DragBrowserToNewTabStrip, this does not have to special-handle
  // IsUsingWindowServices(), since DesktopWIndowTreeHostMus takes care of it.
  attached_widget->GetGestureRecognizer()->TransferEventsTo(
      attached_widget->GetNativeView(), dragged_widget->GetNativeView(),
      ui::TransferTouchesBehavior::kDontCancel);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, Detach should release capture; |can_release_capture_| is
  // false on ChromeOS because it can cancel touches, but for this cases
  // the touches are already transferred, so releasing is fine. Without
  // releasing, the capture remains and further touch events can be sent to a
  // wrong target.
  Detach(RELEASE_CAPTURE);
#else
  Detach(can_release_capture_ ? RELEASE_CAPTURE : DONT_RELEASE_CAPTURE);
#endif

  dragged_widget->SetCanAppearInExistingFullscreenSpaces(true);
  dragged_widget->SetVisibilityChangedAnimationsEnabled(false);
  Attach(dragged_browser_view->tabstrip()->GetDragContext(), gfx::Point());
  AdjustBrowserAndTabBoundsForDrag(tab_area_width, point_in_screen,
                                   &drag_offset, &drag_bounds);
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

  // RunMoveLoop can be called reentrantly from within another RunMoveLoop,
  // in which case the observation is already established.
  widget_observation_.Reset();
  widget_observation_.Observe(move_loop_widget_);
  current_state_ = DragState::kDraggingWindow;
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  if (can_release_capture_) {
    // Running the move loop releases mouse capture, which triggers destroying
    // the drag loop. Release mouse capture now while the DragController is not
    // owned by the TabDragContext.
    attached_context_->ReleaseDragController();
    attached_context_->AsView()->GetWidget()->ReleaseCapture();
    attached_context_->OwnDragController(this);
  }
  const views::Widget::MoveLoopSource move_loop_source =
      event_source_ == EVENT_SOURCE_MOUSE
          ? views::Widget::MoveLoopSource::kMouse
          : views::Widget::MoveLoopSource::kTouch;
  const views::Widget::MoveLoopEscapeBehavior escape_behavior =
      is_dragging_new_browser_
          ? views::Widget::MoveLoopEscapeBehavior::kHide
          : views::Widget::MoveLoopEscapeBehavior::kDontHide;
  views::Widget::MoveLoopResult result = move_loop_widget_->RunMoveLoop(
      drag_offset, move_loop_source, escape_behavior);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::NotificationService::NoDetails());

  if (!ref)
    return;

  if (move_loop_widget_ &&
      widget_observation_.IsObservingSource(move_loop_widget_)) {
    widget_observation_.Reset();
  }
  move_loop_widget_ = nullptr;

  if (current_state_ == DragState::kDraggingWindow) {
    current_state_ = DragState::kWaitingToStop;
  }

  if (current_state_ == DragState::kWaitingToDragTabs) {
    DCHECK(tab_strip_to_attach_to_after_exit_);
    gfx::Point point_in_screen(GetCursorScreenPoint());
    Detach(DONT_RELEASE_CAPTURE);
    Attach(tab_strip_to_attach_to_after_exit_, point_in_screen);
    current_state_ = DragState::kDraggingTabs;
    // Move the tabs into position.
    MoveAttached(point_in_screen, true);
    attached_context_->AsView()->GetWidget()->Activate();
    // Activate may trigger a focus loss, destroying us.
    if (!ref)
      return;
    tab_strip_to_attach_to_after_exit_ = nullptr;
  } else if (current_state_ == DragState::kWaitingToStop) {
    EndDrag(result == views::Widget::MoveLoopResult::kCanceled
                ? END_DRAG_CANCEL
                : END_DRAG_COMPLETE);
  }
}

gfx::Rect TabDragController::GetDraggedViewTabStripBounds(
    const gfx::Point& tab_strip_point) {
  // attached_view is null when inserting into a new context.
  if (source_view_drag_data()->attached_view) {
    std::vector<gfx::Rect> all_bounds =
        attached_context_->CalculateBoundsForDraggedViews(attached_views_);
    int total_width = all_bounds.back().right() - all_bounds.front().x();
    return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(), total_width,
                     source_view_drag_data()->attached_view->height());
  }

  return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                   attached_context_->GetActiveTabWidth(),
                   GetLayoutConstant(TAB_HEIGHT));
}

gfx::Point TabDragController::GetAttachedDragPoint(
    const gfx::Point& point_in_screen) {
  DCHECK(attached_context_);  // The tab must be attached.

  gfx::Point tab_loc(point_in_screen);
  views::View::ConvertPointFromScreen(attached_context_->AsView(), &tab_loc);
  const int x = attached_context_->AsView()->GetMirroredXInView(tab_loc.x()) -
                mouse_offset_.x();

  const int max_x = attached_context_->GetTabDragAreaWidth() -
                    TabStrip::GetSizeNeededForViews(attached_views_);
  return gfx::Point(base::ClampToRange(x, 0, max_x), 0);
}

std::vector<TabSlotView*> TabDragController::GetViewsMatchingDraggedContents(
    TabDragContext* context) {
  TabStripModel* model = attached_context_->GetTabStripModel();
  std::vector<TabSlotView*> views;
  for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
    int model_index = model->GetIndexOfWebContents(drag_data_[i].contents);
    if (model_index == TabStripModel::kNoTab)
      return std::vector<TabSlotView*>();
    views.push_back(context->GetTabAt(model_index));
  }
  if (header_drag_)
    views.insert(views.begin(), context->GetTabGroupHeader(group_.value()));
  return views;
}

void TabDragController::EndDragImpl(EndDragType type) {
  DragState previous_state = current_state_;
  current_state_ = DragState::kStopped;
  attached_context_tabs_closed_tracker_.reset();

  bring_to_front_timer_.Stop();
  move_stacked_timer_.Stop();

  if (type != TAB_DESTROYED) {
    // We only finish up the drag if we were actually dragging. If start_drag_
    // is false, the user just clicked and released and didn't move the mouse
    // enough to trigger a drag.
    if (previous_state != DragState::kNotStarted) {
      // After the drag ends, sometimes it shouldn't restore the focus, because
      // - if |attached_context_| is showing in overview mode, overview mode
      //   may be ended unexpectly because of the window activation.
      // - Some dragging gesture (like fling down) minimizes the window, but the
      //   window activation cancels minimized status. See
      //   https://crbug.com/902897
      if (!IsShowingInOverview(attached_context_) &&
          !attached_context_->AsView()->GetWidget()->IsMinimized()) {
        RestoreFocus();
      }

      GetAttachedBrowserWidget()->SetCanAppearInExistingFullscreenSpaces(false);
      if (type == CANCELED)
        RevertDrag();
      else
        CompleteDrag();
    }
  } else if (drag_data_.size() > 1) {
    initial_selection_model_.Clear();
    if (previous_state != DragState::kNotStarted)
      RevertDrag();
  }  // else case the only tab we were dragging was deleted. Nothing to do.

  // Clear tab dragging info after the complete/revert as CompleteDrag() may
  // need to use some of the properties.
  ClearTabDraggingInfo();

  // Clear out drag data so we don't attempt to do anything with it.
  drag_data_.clear();

  TabDragContext* owning_context =
      attached_context_ ? attached_context_ : source_context_;
  owning_context->DestroyDragController();
}

void TabDragController::PerformDeferredAttach() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  TabDragContext* deferred_target_context =
      deferred_target_context_observer_->deferred_target_context();
  if (!deferred_target_context)
    return;

  DCHECK_NE(deferred_target_context, attached_context_);

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

  // GetCursorScreenPoint() needs to be called before Detach() is called as
  // GetCursorScreenPoint() may use the current attached tabstrip to get the
  // touch event position but Detach() sets attached tabstrip to nullptr.
  // On ChromeOS, the gesture state is already cleared and so
  // GetCursorScreenPoint() will fail to obtain the last touch location.
  // Therefore it uses the last remembered location instead.
  const gfx::Point current_screen_point = (event_source_ == EVENT_SOURCE_TOUCH)
                                              ? last_point_in_screen_
                                              : GetCursorScreenPoint();
  Detach(DONT_RELEASE_CAPTURE);
  // If we're attaching the dragged tabs to an overview window's tabstrip, the
  // tabstrip should not have focus.
  Attach(deferred_target_context, current_screen_point, /*set_capture=*/false);

  SetDeferredTargetTabstrip(nullptr);
  deferred_target_context_observer_.reset();
#endif
}

void TabDragController::RevertDrag() {
  std::vector<TabSlotView*> views;
  if (header_drag_)
    views.push_back(drag_data_[0].attached_view);
  for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents) {
      // Contents is NULL if a tab was destroyed while the drag was under way.
      views.push_back(drag_data_[i].attached_view);
      RevertDragAt(i);
    }
  }

  if (attached_context_) {
    if (did_restore_window_)
      MaximizeAttachedWindow();
    if (attached_context_ == source_context_) {
      source_context_->StoppedDragging(views, initial_tab_positions_,
                                       move_behavior_ == MOVE_VISIBLE_TABS,
                                       false);
      if (header_drag_)
        source_context_->GetTabStripModel()->MoveTabGroup(group_.value());
    } else {
      attached_context_->DraggedTabsDetached();
    }
  }

  // If tabs were closed during this drag, the initial selection might include
  // indices that are out of bounds for the tabstrip now. Reset the selection to
  // include the stille-existing currently dragged WebContentses.
  for (int selection : initial_selection_model_.selected_indices()) {
    if (!source_context_->GetTabStripModel()->ContainsIndex(selection)) {
      initial_selection_model_.Clear();
      break;
    }
  }

  if (initial_selection_model_.empty())
    ResetSelection(source_context_->GetTabStripModel());
  else
    source_context_->GetTabStripModel()->SetSelectionFromModel(
        initial_selection_model_);

  if (source_context_)
    source_context_->AsView()->GetWidget()->Activate();
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
      if (!has_one_valid_tab || i == source_view_index_) {
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
    if (i->source_model_index != TabStripModel::kNoTab)
      selection_model.DecrementFrom(i->source_model_index);
  }
  // We may have cleared out the selection model. Only reset it if it
  // contains something.
  if (selection_model.empty())
    return;

  // Tabs in |source_context_| may have closed since the drag began. In that
  // case, |initial_selection_model_| may include indices that are no longer
  // valid in |source_context_|. Abort restoring the selection if so.
  if (!source_context_->GetTabStripModel()->ContainsIndex(
          *(selection_model.selected_indices().rbegin())))
    return;

  // The anchor/active may have been among the tabs that were dragged out. Force
  // the anchor/active to be valid.
  if (selection_model.anchor() == ui::ListSelectionModel::kUnselectedIndex)
    selection_model.set_anchor(*selection_model.selected_indices().begin());
  if (selection_model.active() == ui::ListSelectionModel::kUnselectedIndex)
    selection_model.set_active(*selection_model.selected_indices().begin());
  source_context_->GetTabStripModel()->SetSelectionFromModel(selection_model);
}

void TabDragController::RevertDragAt(size_t drag_index) {
  DCHECK_NE(current_state_, DragState::kNotStarted);
  DCHECK(source_context_);

  base::AutoReset<bool> setter(&is_mutating_, true);
  TabDragData* data = &(drag_data_[drag_index]);
  // The index we will try to insert the tab at. It may or may not end up at
  // this index, if the source tabstrip has changed since the drag began.
  int target_index = data->source_model_index;
  if (attached_context_) {
    int index = attached_context_->GetTabStripModel()->GetIndexOfWebContents(
        data->contents);
    if (attached_context_ != source_context_) {
      // The Tab was inserted into another TabDragContext. We need to
      // put it back into the original one.
      std::unique_ptr<content::WebContents> detached_web_contents =
          attached_context_->GetTabStripModel()->DetachWebContentsAt(index);
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      source_context_->GetTabStripModel()->InsertWebContentsAt(
          target_index, std::move(detached_web_contents),
          (data->pinned ? TabStripModel::ADD_PINNED : 0));
    } else {
      // The Tab was moved within the TabDragContext where the drag
      // was initiated. Move it back to the starting location.

      // If the target index is to the right, then other unreverted tabs are
      // occupying indices between this tab and the target index. Those
      // unreverted tabs will later be reverted to the right of the target
      // index, so we skip those indices.
      if (target_index > index) {
        for (size_t i = drag_index + 1; i < drag_data_.size(); ++i) {
          if (drag_data_[i].contents)
            ++target_index;
        }
      }
      source_context_->GetTabStripModel()->MoveWebContentsAt(
          index, target_index, false);
    }
  } else {
    // The Tab was detached from the TabDragContext where the drag
    // began, and has not been attached to any other TabDragContext.
    // We need to put it back into the source TabDragContext.
    source_context_->GetTabStripModel()->InsertWebContentsAt(
        target_index, std::move(data->owned_contents),
        (data->pinned ? TabStripModel::ADD_PINNED : 0));
  }
  TabStripModel* source_model = source_context_->GetTabStripModel();
  source_model->UpdateGroupForDragRevert(
      source_model->GetIndexOfWebContents(data->contents),
      data->tab_group_data.has_value()
          ? base::Optional<tab_groups::TabGroupId>{data->tab_group_data.value()
                                                       .group_id}
          : base::nullopt,
      data->tab_group_data.has_value()
          ? base::Optional<
                tab_groups::TabGroupVisualData>{data->tab_group_data.value()
                                                    .group_visual_data}
          : base::nullopt);
}

void TabDragController::CompleteDrag() {
  DCHECK_NE(current_state_, DragState::kNotStarted);

  if (attached_context_) {
    if (is_dragging_new_browser_ || did_restore_window_) {
      if (IsSnapped(attached_context_)) {
        was_source_maximized_ = false;
        was_source_fullscreen_ = false;
      }

      // If source window was maximized - maximize the new window as well.
      if (was_source_maximized_ || was_source_fullscreen_)
        MaximizeAttachedWindow();
    }
    attached_context_->StoppedDragging(
        GetViewsMatchingDraggedContents(attached_context_),
        initial_tab_positions_, move_behavior_ == MOVE_VISIBLE_TABS, true);
  } else {
    // Compel the model to construct a new window for the detached
    // WebContentses.
    views::Widget* widget = source_context_->AsView()->GetWidget();
    gfx::Rect window_bounds(widget->GetRestoredBounds());
    window_bounds.set_origin(GetWindowCreatePoint(last_point_in_screen_));

    base::AutoReset<bool> setter(&is_mutating_, true);

    std::vector<TabStripModelDelegate::NewStripContents> contentses;
    for (size_t i = 0; i < drag_data_.size(); ++i) {
      TabStripModelDelegate::NewStripContents item;
      // We should have owned_contents here, this CHECK is used to gather data
      // for https://crbug.com/677806.
      CHECK(drag_data_[i].owned_contents);
      item.web_contents = std::move(drag_data_[i].owned_contents);
      item.add_types = drag_data_[i].pinned ? TabStripModel::ADD_PINNED
                                            : TabStripModel::ADD_NONE;
      contentses.push_back(std::move(item));
    }

    Browser* new_browser =
        source_context_->GetTabStripModel()
            ->delegate()
            ->CreateNewStripWithContents(std::move(contentses), window_bounds,
                                         widget->IsMaximized());
    ResetSelection(new_browser->tab_strip_model());
    new_browser->window()->Show();
  }

  if (header_drag_) {
    // Manually reset the selection to just the active tab in the group.
    // Otherwise, it's easy to accidentally delete the fully-selected group
    // by dragging on any of its still-selected members.
    TabStripModel* model = attached_context_
                               ? attached_context_->GetTabStripModel()
                               : source_context_->GetTabStripModel();
    ui::ListSelectionModel selection;
    int index = model->GetIndexOfWebContents(drag_data_[1].contents);
    // The tabs in the group may have been closed during the drag.
    if (index != TabStripModel::kNoTab) {
      selection.AddIndexToSelection(index);
      selection.set_active(index);
      selection.set_anchor(index);
      model->SetSelectionFromModel(selection);
    }
  }
}

void TabDragController::MaximizeAttachedWindow() {
  GetAttachedBrowserWidget()->Maximize();
#if defined(OS_MAC)
  if (was_source_fullscreen_)
    GetAttachedBrowserWidget()->SetFullscreen(true);
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

void TabDragController::BringWindowUnderPointToFront(
    const gfx::Point& point_in_screen) {
  gfx::NativeWindow window;
  if (GetLocalProcessWindow(point_in_screen, true, &window) ==
      Liveness::DELETED) {
    return;
  }

  // Only bring browser windows to front - only windows with a
  // TabDragContext can be tab drag targets.
  if (!CanAttachTo(window))
    return;

  if (window) {
    views::Widget* widget_window =
        views::Widget::GetWidgetForNativeWindow(window);
    if (!widget_window)
      return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    if (current_state_ == DragState::kDraggingWindow)
      attached_context_->AsView()->GetWidget()->StackAtTop();
  }
}

views::Widget* TabDragController::GetAttachedBrowserWidget() {
  return attached_context_->AsView()->GetWidget();
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
    TabDragContext* source,
    const gfx::Point& point_in_screen,
    std::vector<gfx::Rect>* drag_bounds) {
  gfx::Point center(0, source->AsView()->height() / 2);
  views::View::ConvertPointToWidget(source->AsView(), &center);
  gfx::Rect new_bounds(source->AsView()->GetWidget()->GetRestoredBounds());

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

  if (source->AsView()->GetWidget()->IsMaximized()) {
    // If the restore bounds is really small, we don't want to honor it
    // (dragging a really small window looks wrong), instead make sure the new
    // window is at least 50% the size of the old.
    const gfx::Size max_size(
        source->AsView()->GetWidget()->GetWindowBoundsInScreen().size());
    new_bounds.set_width(std::max(max_size.width() / 2, new_bounds.width()));
    new_bounds.set_height(std::max(max_size.height() / 2, new_bounds.height()));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::TabletMode::Get()->InTabletMode()) {
    new_bounds = GetDraggedBrowserBoundsInTabletMode(
        source->AsView()->GetWidget()->GetNativeWindow());
  }
#endif

  new_bounds.set_y(point_in_screen.y() - center.y());
  switch (GetDetachPosition(point_in_screen)) {
    case DETACH_BEFORE:
      new_bounds.set_x(point_in_screen.x() - center.x());
      new_bounds.Offset(-mouse_offset_.x(), 0);
      break;
    case DETACH_AFTER: {
      gfx::Point right_edge(source->AsView()->width(), 0);
      views::View::ConvertPointToWidget(source->AsView(), &right_edge);
      new_bounds.set_x(point_in_screen.x() - right_edge.x());
      new_bounds.Offset(drag_bounds->back().right() - mouse_offset_.x(), 0);
      OffsetX(-drag_bounds->front().x(), drag_bounds);
      break;
    }
    default:
      break;  // Nothing to do for DETACH_ABOVE_OR_BELOW.
  }

  // Account for the extra space above the tabstrip on restored windows versus
  // maximized windows.
  if (source->AsView()->GetWidget()->IsMaximized()) {
    const auto* frame_view = static_cast<BrowserNonClientFrameView*>(
        source->AsView()->GetWidget()->non_client_view()->frame_view());
    new_bounds.Offset(
        0, frame_view->GetTopInset(false) - frame_view->GetTopInset(true));
  }
  return new_bounds;
}

gfx::Rect TabDragController::CalculateNonMaximizedDraggedBrowserBounds(
    views::Widget* widget,
    const gfx::Point& point_in_screen) {
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::TabletMode::Get()->InTabletMode())
    bounds = GetDraggedBrowserBoundsInTabletMode(widget->GetNativeWindow());
#endif

  // The user has to move the mouse some amount of pixels before the drag
  // starts. Offset the window by this amount so that the relative offset
  // of the initial location is consistent. See https://crbug.com/518740
  bounds.Offset(point_in_screen.x() - start_point_in_screen_.x(),
                point_in_screen.y() - start_point_in_screen_.y());
  return bounds;
}

void TabDragController::AdjustBrowserAndTabBoundsForDrag(
    int tab_area_width,
    const gfx::Point& point_in_screen,
    gfx::Vector2d* drag_offset,
    std::vector<gfx::Rect>* drag_bounds) {
  attached_context_->ForceLayout();
  const int dragged_context_width = attached_context_->GetTabDragAreaWidth();

  // If the new tabstrip region is smaller than the old, resize the tabs.
  if (dragged_context_width < tab_area_width) {
    const float leading_ratio =
        drag_bounds->front().x() / float{tab_area_width};
    *drag_bounds =
        attached_context_->CalculateBoundsForDraggedViews(attached_views_);

    if (drag_bounds->back().right() < dragged_context_width) {
      const int delta_x = std::min(
          int{(leading_ratio * dragged_context_width)},
          dragged_context_width -
              (drag_bounds->back().right() - drag_bounds->front().x()));
      OffsetX(delta_x, drag_bounds);
    }

    // Reposition the restored window such that the tab that was dragged remains
    // under the mouse cursor.
    gfx::Rect tab_bounds = (*drag_bounds)[source_view_index_];
    gfx::Point offset(
        base::ClampRound(tab_bounds.width() * offset_to_width_ratio_) +
            tab_bounds.x(),
        0);
    views::View::ConvertPointToWidget(attached_context_->AsView(), &offset);
    gfx::Rect bounds = GetAttachedBrowserWidget()->GetWindowBoundsInScreen();
    bounds.set_x(point_in_screen.x() - offset.x());
    GetAttachedBrowserWidget()->SetBounds(bounds);
    *drag_offset = point_in_screen - bounds.origin();
  }
  attached_context_->SetBoundsForDrag(attached_views_, *drag_bounds);
}

Browser* TabDragController::CreateBrowserForDrag(
    TabDragContext* source,
    const gfx::Point& point_in_screen,
    gfx::Vector2d* drag_offset,
    std::vector<gfx::Rect>* drag_bounds) {
  gfx::Rect new_bounds(
      CalculateDraggedBrowserBounds(source, point_in_screen, drag_bounds));
  *drag_offset = point_in_screen - new_bounds.origin();

  Browser::CreateParams create_params =
      BrowserView::GetBrowserViewForNativeWindow(
          GetAttachedBrowserWidget()->GetNativeWindow())
          ->browser()
          ->create_params();
  create_params.user_gesture = true;
  create_params.in_tab_dragging = true;
  create_params.initial_bounds = new_bounds;
  // Do not copy attached window's show state as the attached window might be a
  // maximized or fullscreen window and we do not want the newly created browser
  // window is a maximized or fullscreen window since it will prevent window
  // moving/resizing on Chrome OS. See crbug.com/1023871 for details.
  create_params.initial_show_state = ui::SHOW_STATE_DEFAULT;

  // Don't copy the initial workspace since the *current* workspace might be
  // different and copying the workspace will move the tab to the initial one.
  create_params.initial_workspace = "";

  // Don't copy the window name - the user's deliberately creating a new window,
  // which should default to its own auto-generated name, not the same name as
  // the previous window.
  create_params.user_title = "";

  Browser* browser = Browser::Create(create_params);
  is_dragging_new_browser_ = true;
  // If the window is created maximized then the bounds we supplied are ignored.
  // We need to reset them again so they are honored.
  browser->window()->SetBounds(new_bounds);

  return browser;
}

gfx::Point TabDragController::GetCursorScreenPoint() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (event_source_ == EVENT_SOURCE_TOUCH &&
      aura::Env::GetInstance()->is_touch_down()) {
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
  TabDragContext* owning_context =
      attached_context_ ? attached_context_ : source_context_;
  views::View* toplevel_view =
      owning_context->AsView()->GetWidget()->GetContentsView();

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
        attached_context_->AsView()->GetWidget()->GetNativeWindow();
    if (dragged_window)
      exclude.insert(dragged_window);
  }
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  TabDragContext* dragged_context =
      attached_context_ ? attached_context_ : source_context_;
  DCHECK(dragged_context->IsDragSessionActive() &&
         current_state_ != DragState::kStopped);

  aura::Window* dragged_window =
      GetWindowForTabDraggingProperties(dragged_context);
  aura::Window* source_window =
      GetWindowForTabDraggingProperties(source_context_);
  dragged_window->SetProperty(ash::kIsDraggingTabsKey, true);
  if (source_window != dragged_window) {
    dragged_window->SetProperty(ash::kTabDraggingSourceWindowKey,
                                source_window);
  }
#endif
}

void TabDragController::ClearTabDraggingInfo() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  TabDragContext* dragged_context =
      attached_context_ ? attached_context_ : source_context_;
  DCHECK(!dragged_context->IsDragSessionActive() ||
         current_state_ == DragState::kStopped);
  // Do not clear the dragging info properties for a to-be-destroyed window.
  // They will be cleared later in Window's destructor. It's intentional as
  // ash::SplitViewController::TabDraggedWindowObserver listens to both
  // OnWindowDestroying() event and the window properties change event, and uses
  // the two events to decide what to do next.
  if (dragged_context->GetTabStripModel()->empty())
    return;

  aura::Window* dragged_window =
      GetWindowForTabDraggingProperties(dragged_context);
  dragged_window->ClearProperty(ash::kIsDraggingTabsKey);
  dragged_window->ClearProperty(ash::kTabDraggingSourceWindowKey);
#endif
}

void TabDragController::UpdateGroupForDraggedTabs() {
  TabStripModel* attached_model = attached_context_->GetTabStripModel();

  const ui::ListSelectionModel::SelectedIndices& selected =
      attached_model->selection_model().selected_indices();

  // Pinned tabs cannot be grouped, so we only change the group membership of
  // unpinned tabs.
  std::vector<int> selected_unpinned;
  for (const int& selected_index : selected) {
    if (!attached_model->IsTabPinned(selected_index))
      selected_unpinned.push_back(selected_index);
  }

  if (selected_unpinned.empty())
    return;

  const base::Optional<tab_groups::TabGroupId> updated_group =
      GetTabGroupForTargetIndex(selected_unpinned);

  if (updated_group == attached_model->GetTabGroupForTab(selected_unpinned[0]))
    return;

  attached_model->MoveTabsAndSetGroup(selected_unpinned, selected_unpinned[0],
                                      updated_group);
}

base::Optional<tab_groups::TabGroupId>
TabDragController::GetTabGroupForTargetIndex(const std::vector<int>& selected) {
  // Indices in {selected} are always ordered in ascending order and should all
  // be consecutive.
  DCHECK_EQ(selected.back() - selected.front() + 1, int{selected.size()});
  const TabStripModel* attached_model = attached_context_->GetTabStripModel();

  const int left_tab_index = selected.front() - 1;

  const base::Optional<tab_groups::TabGroupId> left_group =
      attached_model->GetTabGroupForTab(left_tab_index);
  const base::Optional<tab_groups::TabGroupId> right_group =
      attached_model->GetTabGroupForTab(selected.back() + 1);
  const base::Optional<tab_groups::TabGroupId> current_group =
      attached_model->GetTabGroupForTab(selected[0]);

  if (left_group == right_group)
    return left_group;

  // If the tabs on the left and right have different group memberships,
  // including if one is ungrouped or nonexistent, change the group of the
  // dragged tab based on whether it is "leaning" toward the left or the
  // right of the gap. If the tab is centered in the gap, make the tab
  // ungrouped.

  const Tab* left_most_selected_tab =
      attached_context_->GetTabAt(selected.front());

  const int buffer = left_most_selected_tab->width() / 4;

  // The tab's bounds are larger than what visually appears in order to include
  // space for the rounded feet. Adding {tab_left_inset} to the horiztonal
  // bounds of the tab results in the x position that would be drawn when there
  // are no feet showing.
  const int tab_left_inset = TabStyle::GetTabOverlap() / 2;

  // Use the left edge for a reliable fallback, e.g. if this is the leftmost
  // tab or there is a group header to the immediate left.
  int left_edge =
      attached_model->ContainsIndex(left_tab_index)
          ? attached_context_->GetTabAt(left_tab_index)->bounds().right() -
                tab_left_inset
          : tab_left_inset;

  // Extra polish: Prefer staying in an existing group, if any. This prevents
  // tabs at the edge of the group from flickering between grouped and
  // ungrouped. It also gives groups a slightly "sticky" feel while dragging.
  if (left_group.has_value() && left_group == current_group)
    left_edge += buffer;
  if (right_group.has_value() && right_group == current_group &&
      left_edge > tab_left_inset) {
    left_edge -= buffer;
  }

  int left_most_selected_x_position =
      left_most_selected_tab->x() + tab_left_inset;

  if ((left_most_selected_x_position <= left_edge - buffer) &&
      left_group.has_value() &&
      !attached_model->IsGroupCollapsed(left_group.value())) {
    return left_group;
  }
  if ((left_most_selected_x_position >= left_edge + buffer) &&
      right_group.has_value() &&
      !attached_model->IsGroupCollapsed(right_group.value())) {
    return right_group;
  }
  return base::nullopt;
}

bool TabDragController::CanAttachTo(gfx::NativeWindow window) {
  if (!window)
    return false;

  BrowserView* other_browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  if (!other_browser_view)
    return false;
  Browser* other_browser = other_browser_view->browser();

  // Do not allow dragging into a window with a modal dialog, it causes a
  // weird behavior.  See crbug.com/336691
#if defined(USE_AURA)
  if (wm::GetModalTransient(window))
    return false;
#else
  TabStripModel* model = other_browser->tab_strip_model();
  DCHECK(model);
  if (model->IsTabBlocked(model->active_index()))
    return false;
#endif

  // We don't allow drops on windows that don't have tabstrips.
  if (!other_browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP))
    return false;

  Browser* browser = BrowserView::GetBrowserViewForNativeWindow(
                         GetAttachedBrowserWidget()->GetNativeWindow())
                         ->browser();

  // Profiles must be the same.
  if (other_browser->profile() != browser->profile())
    return false;

  // Ensure that browser types and app names are the same.
  if (other_browser->type() != browser->type() ||
      (browser->is_type_app() &&
       browser->app_name() != other_browser->app_name())) {
    return false;
  }

  return true;
}

void TabDragController::SetDeferredTargetTabstrip(
    TabDragContext* deferred_target_context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!deferred_target_context_observer_) {
    deferred_target_context_observer_ =
        std::make_unique<DeferredTargetTabstripObserver>();
  }
  deferred_target_context_observer_->SetDeferredTargetTabstrip(
      deferred_target_context);
#endif
}
