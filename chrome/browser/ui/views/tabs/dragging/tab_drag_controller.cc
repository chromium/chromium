// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <variant>

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/range/range.h"
#include "ui/views/drag_utils.h"
#include "ui/views/event_monitor.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/window_properties.h"  // nogncheck
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_number_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "components/remote_cocoa/browser/window.h"
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

// A dragged window is forced to be a bit smaller than maximized bounds during a
// drag. This prevents the dragged browser widget from getting maximized at
// creation and makes it easier to drag tabs out of a restored window that had
// maximized size.
constexpr int kMaximizedWindowInset = 10;  // DIPs.

constexpr char kTabDraggingPresentationTimeHistogram[] =
    "Browser.TabDragging.PresentationTime";
constexpr char kTabDraggingPresentationTimeMaxHistogram[] =
    "Browser.TabDragging.PresentationTimeMax";
constexpr char kDragToNewBrowserPresentationTimeHistogram[] =
    "Browser.TabDragging.DragToNewBrowserPresentationTime";

#if BUILDFLAG(IS_CHROMEOS)

// Returns the aura::Window which stores the window properties for tab-dragging.
aura::Window* GetWindowForTabDraggingProperties(const TabDragContext* context) {
  return context ? context->GetWidget()->GetNativeWindow() : nullptr;
}

// Returns true if `context` browser window is snapped.
bool IsSnapped(const TabDragContext* context) {
  DCHECK(context);
  chromeos::WindowStateType type =
      GetWindowForTabDraggingProperties(context)->GetProperty(
          chromeos::kWindowStateTypeKey);
  return type == chromeos::WindowStateType::kPrimarySnapped ||
         type == chromeos::WindowStateType::kSecondarySnapped;
}

#else

bool IsSnapped(const TabDragContext* context) {
  return false;
}

#endif  // BUILDFLAG(IS_CHROMEOS)

gfx::Rect GetTabstripScreenBounds(const TabDragContext* context) {
  const views::View* view = context;
  gfx::Point view_topleft;
  views::View::ConvertPointToScreen(view, &view_topleft);
  gfx::Rect view_screen_bounds = view->GetLocalBounds();
  view_screen_bounds.Offset(view_topleft.x(), view_topleft.y());
  return view_screen_bounds;
}

// Returns true if `bounds` contains the y-coordinate `y`. The y-coordinate
// of `bounds` is adjusted by `vertical_adjustment`.
bool DoesRectContainVerticalPointExpanded(const gfx::Rect& bounds,
                                          int vertical_adjustment,
                                          int y) {
  int upper_threshold = bounds.bottom() + vertical_adjustment;
  int lower_threshold = bounds.y() - vertical_adjustment;
  return y >= lower_threshold && y <= upper_threshold;
}

// Adds `x_offset` to all the rectangles in `rects`.
void OffsetX(int x_offset, std::vector<gfx::Rect>* rects) {
  if (x_offset == 0) {
    return;
  }

  for (auto& rect : *rects) {
    rect.set_x(rect.x() + x_offset);
  }
}

void UpdateSystemDnDDragImage(TabDragContext* attached_context,
                              const gfx::ImageSkia& image) {
#if BUILDFLAG(IS_LINUX)
  VLOG(1) << __func__ << " image size=" << image.size().ToString();
  aura::Window* root_window =
      attached_context->GetWidget()->GetNativeWindow()->GetRootWindow();
  if (aura::client::GetDragDropClient(root_window)) {
    aura::client::GetDragDropClient(root_window)
        ->UpdateDragImage(image, {image.width() / 2, image.height() / 2});
  }
#endif  // BUILDFLAG(IS_LINUX)
}

class DefaultTabDragPointResolver : public TabDragPointResolver {
 public:
  DefaultTabDragPointResolver() = default;
  ~DefaultTabDragPointResolver() override = default;

  TabDragDelegate* GetDragTarget(BrowserView& browser_view,
                                 const gfx::Point& point_in_screen) override {
    return browser_view.GetTabDragDelegate(point_in_screen);
  }
};

TabDragPointResolver* g_tab_drag_point_resolver_ = nullptr;

TabDragPointResolver* GetTabDragPointResolver() {
  if (!g_tab_drag_point_resolver_) {
    static DefaultTabDragPointResolver resolver;
    g_tab_drag_point_resolver_ = &resolver;
  }
  return g_tab_drag_point_resolver_;
}

}  // namespace

// EventTracker installs an event monitor and ends the drag when it receives any
// key event, or a mouse release event during a system DnD session.
class EventTracker : public ui::EventObserver {
 public:
  EventTracker(base::OnceClosure end_drag_callback,
               base::OnceClosure revert_drag_callback,
               gfx::NativeWindow context)
      : end_drag_callback_(std::move(end_drag_callback)),
        revert_drag_callback_(std::move(revert_drag_callback)) {
    event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
        this, context,
        {ui::EventType::kKeyPressed, ui::EventType::kMouseReleased});
  }
  EventTracker(const EventTracker&) = delete;
  EventTracker& operator=(const EventTracker&) = delete;
  ~EventTracker() override = default;

 private:
  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    if (event.IsKeyEvent()) {
      if (event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE &&
          revert_drag_callback_) {
        std::move(revert_drag_callback_).Run();
      } else if (event.AsKeyEvent()->key_code() != ui::VKEY_ESCAPE &&
                 end_drag_callback_) {
        std::move(end_drag_callback_).Run();
      }
    } else {
      CHECK(event.type() == ui::EventType::kMouseReleased);
      if (TabDragController::IsSystemDnDSessionRunning()) {
        TabDragController::OnSystemDnDEnded();
      }
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

  const raw_ptr<TabStripModel, AcrossTasksDanglingUntriaged> tab_strip_;
  const raw_ptr<TabDragController, AcrossTasksDanglingUntriaged> parent_;
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
        for (const auto& contents : change.GetRemove()->contents) {
          parent_->OnActiveStripWebContentsRemoved(contents.contents);
        }
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
  const raw_ptr<TabDragController, DanglingUntriaged> parent_;
};

///////////////////////////////////////////////////////////////////////////////
// TabDragController, public:

// static
const int TabDragController::kTouchVerticalDetachMagnetism = 50;

// static
const int TabDragController::kVerticalDetachMagnetism = 15;

TabDragController::TabDragController()
    : current_state_(DragState::kNotStarted),
      source_context_(nullptr),
      attached_context_(nullptr),
      can_release_capture_(true),
      offset_to_width_ratio_(0),
      old_focused_view_tracker_(std::make_unique<views::ViewTracker>()),
      detach_behavior_(DetachBehavior::kDetachable),
      is_dragging_new_browser_(false),
      was_source_maximized_(false),
      was_source_fullscreen_(false),
      did_restore_window_(false),
      tab_strip_to_attach_to_after_exit_(nullptr),
      move_loop_widget_(nullptr),
      is_mutating_(false) {
  g_tab_drag_controller = this;
}

TabDragController::~TabDragController() {
  if (g_tab_drag_controller == this) {
    g_tab_drag_controller = nullptr;
  }

  widget_observation_.Reset();

  if (current_state_ == DragState::kDraggingWindow) {
    VLOG(1) << "EndMoveLoop in TabDragController dtor";
    GetAttachedBrowserWidget()->EndMoveLoop();
  }

  if (event_source_ == ui::mojom::DragEventSource::kTouch) {
    TabDragContext* capture_context =
        attached_context_ ? attached_context_.get() : source_context_.get();
    capture_context->GetWidget()->ReleaseCapture();
  }
  CHECK(!IsInObserverList());

  DCHECK(!expect_stay_alive_)
      << "TabDragController was destroyed when it shouldn't have been. Check "
         "up the stack for reentrancy.";
}

TabDragController::Liveness TabDragController::Init(
    TabDragContext* source_context,
    TabSlotView* source_view,
    const std::vector<TabSlotView*>& dragging_views,
    const gfx::Point& mouse_offset,
    int source_view_offset,
    ui::ListSelectionModel initial_selection_model,
    ui::mojom::DragEventSource event_source) {
  DCHECK(!dragging_views.empty());
  DCHECK(base::Contains(dragging_views, source_view));

  // There's some rare condition on Windows where we are destroying ourselves
  // during Init() at some point. It's unclear how this happens, so we'll take
  // the nuke it from orbit approach and use a weak pointer throughout Init().
  // TODO(b/324993098): Revert this and use weak pointers in more targeted ways
  // once we know where the self-destruction is happening.
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());

  ref->source_context_ = source_context;
  ref->was_source_maximized_ = source_context->GetWidget()->IsMaximized();
  ref->was_source_fullscreen_ = source_context->GetWidget()->IsFullscreen();
  ref->presentation_time_recorder_ =
      ui::CreatePresentationTimeHistogramRecorder(
          source_context->GetWidget()->GetCompositor(),
          kTabDraggingPresentationTimeHistogram,
          kTabDraggingPresentationTimeMaxHistogram,
          ui::PresentationTimeRecorder::BucketParams::CreateWithMaximum(
              base::Seconds(10)));
  // Do not release capture when transferring capture between widgets on:
  // - Desktop Linux
  //     Mouse capture is not synchronous on desktop Linux. Chrome makes
  //     transferring capture between widgets without releasing capture appear
  //     synchronous on desktop Linux, so use that.
  // - ChromeOS Ash
  //     Releasing capture on Ash cancels gestures so avoid it.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ref->can_release_capture_ = false;
#endif
  ref->start_point_in_screen_ =
      gfx::Point(source_view_offset, mouse_offset.y());
  views::View::ConvertPointToScreen(source_view,
                                    &(ref->start_point_in_screen_));
  ref->event_source_ = event_source;
  ref->last_point_in_screen_ = start_point_in_screen_;
  // Detachable tabs are not supported on Mac if the window is an out-of-process
  // (remote_cocoa) window, i.e. a PWA window.
  // TODO(crbug.com/40128833): Make detachable tabs work in PWAs on Mac.
#if BUILDFLAG(IS_MAC)
  if (ref->source_context_->GetWidget() &&
      remote_cocoa::IsWindowRemote(
          ref->source_context_->GetWidget()->GetNativeWindow())) {
    ref->detach_behavior_ = DetachBehavior::kNotDetachable;
  }
#else
  // Tabs should not be detachable from the window if any of the following are
  // true:
  // 1. The app window is locked for OnTask. Not applicable for web browser
  //    scenarios.
  // 2. The dragged tab strip exists in a PWA, and any of the dragged views
  //    are the Pinned Home tab.
  Browser* source_browser = BrowserView::GetBrowserViewForNativeWindow(
                                source_context->GetWidget()->GetNativeWindow())
                                ->browser();
#if BUILDFLAG(IS_CHROMEOS)
  if (source_browser->IsLockedForOnTask()) {
    ref->detach_behavior_ = DetachBehavior::kNotDetachable;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  if (source_browser->app_controller() &&
      source_browser->app_controller()->has_tab_strip() &&
      web_app::HasPinnedHomeTab(source_browser->tab_strip_model())) {
    for (TabSlotView* dragging_view : dragging_views) {
      if (source_context->GetIndexOf(dragging_view) == 0) {
        ref->detach_behavior_ = DetachBehavior::kNotDetachable;
      }
    }
  }
#endif  // BUILDFLAG(IS_MAC)

  ref->source_context_emptiness_tracker_ =
      std::make_unique<SourceTabStripEmptinessTracker>(
          ref->source_context_->GetTabStripModel(), this);

  if (source_view->GetTabSlotViewType() ==
      TabSlotView::ViewType::kTabGroupHeader) {
    const TabStripModel* tab_strip_model =
        ref->source_context_->GetTabStripModel();
    const tab_groups::TabGroupId group_id = source_view->group().value();
    const std::optional<tab_groups::TabGroupId> active_group_id =
        tab_strip_model->GetActiveTab()->GetGroup();
    gfx::Range group_range =
        tab_strip_model->group_model()->GetTabGroup(group_id)->ListTabs();
    const int active_tab_index_within_group =
        (active_group_id.has_value() && active_group_id.value() == group_id &&
         !group_range.is_empty())
            ? tab_strip_model->active_index() - group_range.GetMin()
            : 0;
    ref->drag_data_.group_drag_data_ = std::make_optional<GroupDragData>(
        group_id, active_tab_index_within_group);
  }

  for (TabSlotView* dragging_view : dragging_views) {
    ref->drag_data_.tab_drag_data_.emplace_back(source_context_, dragging_view);
  }
  ref->drag_data_.source_view_index_ =
      std::ranges::find(dragging_views, source_view) - dragging_views.begin();

  // Listen for Esc key presses and mouse releases.
  ref->event_tracker_ = std::make_unique<EventTracker>(
      base::BindOnce(&TabDragController::EndDrag, base::Unretained(this),
                     EndDragReason::kComplete),
      base::BindOnce(&TabDragController::EndDrag, base::Unretained(this),
                     EndDragReason::kCancel),
      ref->source_context_->GetWidget()->GetNativeWindow());

  if (source_view->width() > 0) {
    ref->offset_to_width_ratio_ =
        static_cast<float>(
            source_view->GetMirroredXInView(source_view_offset)) /
        source_view->width();
  }
  ref->initial_selection_model_ = std::move(initial_selection_model);

  ref->window_finder_ = std::make_unique<WindowFinder>();

  // Start listening for tabs to be closed or replaced in `source_context_`, in
  // case this happens before the mouse is moved enough to fully start the drag.
  // See crbug/1445776.
  ref->attached_context_tabs_closed_tracker_ =
      std::make_unique<DraggedTabsClosedTracker>(
          ref->source_context_->GetTabStripModel(), this);

  ///////// DO NOT ADD INITIALIZATION CODE BELOW THIS LINE ////////
  // We need to be initialized at this point because SetCapture may reenter this
  // TabDragController and we do not want to deal with a partially-uninitialized
  // object at that point. This is already too complicated.

  // Gestures don't automatically do a capture. We don't allow multiple drags at
  // the same time, so we explicitly capture.
  if (event_source == ui::mojom::DragEventSource::kTouch) {
    // Taking capture involves OS calls which may reentrantly call back into
    // Chrome. This may result in the drag ending, destroying `this`. This
    // behavior has been observed on Windows in https://crbug.com/964322 and
    // ChromeOS in https://crbug.com/1431369.
    if (SetCapture(ref->source_context_) == Liveness::kDeleted) {
      return Liveness::kDeleted;
    }
  }

  CHECK(ref);
  return Liveness::kAlive;
}

// static
bool TabDragController::IsAttachedTo(const TabDragContextBase* context) {
  return (g_tab_drag_controller && g_tab_drag_controller->active() &&
          g_tab_drag_controller->attached_context() == context);
}

// static
bool TabDragController::IsActive() {
  return g_tab_drag_controller && g_tab_drag_controller->active();
}

// static
bool TabDragController::IsSystemDnDSessionRunning() {
  return g_tab_drag_controller &&
         g_tab_drag_controller->system_drag_and_drop_session_running_;
}

// static
void TabDragController::OnSystemDnDUpdated(const ui::DropTargetEvent& event) {
  CHECK(IsSystemDnDSessionRunning());
  VLOG(2) << __func__ << " event=" << event.ToString();
  // It is important to use the event's root location instead of its location.
  // The latter may have been transformed to be relative to a child window, e.g.
  // Aura's clipping window. But we need the location relative to the browser
  // window, i.e. the root location.
  // ignore the Liveness; it's fine if `g_tab_drag_controller` dies here.
  std::ignore = g_tab_drag_controller->Drag(event.root_location());
}

// static
void TabDragController::OnSystemDnDExited() {
  CHECK(IsSystemDnDSessionRunning());
  VLOG(1) << __func__;
  // Call Drag() with a location that is definitely out of the tab strip.
  // ignore the Liveness; it's fine if `g_tab_drag_controller` dies here.
  std::ignore = g_tab_drag_controller->Drag(
      {g_tab_drag_controller->last_point_in_screen_.x(),
       g_tab_drag_controller->GetOutOfBoundsYCoordinate()});
}

// static
void TabDragController::OnSystemDnDEnded() {
  CHECK(IsSystemDnDSessionRunning());
  VLOG(1) << __func__;
  // Set this to prevent us from cancelling the drag again. The platform might
  // not have finished processing the drag end, and requesting to cancel the
  // drag might put us in an infinite loop of being notified about the drag end,
  // requesting to cancel the drag, being notified again, and so on.
  g_tab_drag_controller->system_drag_and_drop_session_running_ = false;
  g_tab_drag_controller->EndDrag(EndDragReason::kComplete);
}

// static
TabDragContext* TabDragController::GetSourceContext() {
  return g_tab_drag_controller ? g_tab_drag_controller->source_context_.get()
                               : nullptr;
}

void TabDragController::TabWasAdded() {
  // Stop dragging when a new tab is added and dragging a window, unless we're
  // doing so ourselves (e.g. while attaching to a new browser). Doing otherwise
  // results in a confusing state if the user attempts to reattach. We could
  // allow this and update ourselves during the add, but this comes up
  // infrequently enough that it's not worth the complexity.
  // Note: When we're in the kDraggingUsingSystemDnD state, this method being
  // called means a tab was added to the source window of the drag. We don't
  // need to cancel the drag in that case.
  if (current_state_ == DragState::kDraggingWindow && !is_mutating_) {
    EndDrag(EndDragReason::kComplete);
  }
}

void TabDragController::OnTabWillBeRemoved(content::WebContents* contents) {
  // End the drag before we remove a tab that's being dragged, to avoid
  // complex special cases that could result.
  if (!CanRemoveTabDuringDrag(contents)) {
    EndDrag(EndDragReason::kComplete);
  }
}

bool TabDragController::CanRemoveTabDuringDrag(
    content::WebContents* contents) const {
  // Tab removal can happen without interrupting dragging only if either a) the
  // tab isn't part of the drag or b) we're doing the removal ourselves.
  return !IsDraggingTab(contents) || is_mutating_;
}

bool TabDragController::CanRestoreFullscreenWindowDuringDrag() const {
#if BUILDFLAG(IS_MAC)
  // On macOS in immersive fullscreen mode restoring the window moves the tab
  // strip between widgets breaking a number of assumptions during the drag.
  // Disable window restoration during a drag while in immersive fullscreen.
  return !base::FeatureList::IsEnabled(features::kImmersiveFullscreen);
#else
  return true;
#endif
}

TabDragController::Liveness TabDragController::Drag(
    const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::Drag", "point_in_screen",
               point_in_screen.ToString());

  // If we're in `kWaitingToExitRunLoop` or `kWaitingToDragTabs`, then we have
  // asked to exit the nested run loop, but are still in it. We should ignore
  // any events until we actually exit the nested run loop, to avoid potentially
  // starting another one (see https://crbug.com/41493121). Similary, if we're
  // kStopped but haven't been destroyed yet, we should ignore events.
  if (current_state_ == DragState::kWaitingToExitRunLoop ||
      current_state_ == DragState::kWaitingToDragTabs ||
      current_state_ == DragState::kStopped) {
    return Liveness::kAlive;
  }

  bring_to_front_timer_.Stop();

  if (current_state_ == DragState::kNotStarted) {
    if (!CanStartDrag(point_in_screen)) {
      return Liveness::kAlive;  // User hasn't dragged far enough yet.
    }

    // If any of the tabs have disappeared (e.g. closed or discarded), cancel
    // the drag session. See crbug.com/1445776.
    if (GetViewsMatchingDraggedContents(source_context_).empty()) {
      EndDrag(EndDragReason::kCancel);
      return Liveness::kDeleted;
    }

    // On windows SaveFocus() may trigger a capture lost, which destroys us.
    {
      const Liveness alive = SaveFocus();
      if (alive == Liveness::kDeleted) {
        return Liveness::kDeleted;
      }
    }

    StartDrag();

    if (drag_data_.num_dragging_tabs() ==
        source_context_->GetTabStripModel()->count()) {
      if (ShouldDragWindowUsingSystemDnD()) {
        return StartSystemDnDSessionIfNecessary(attached_context_,
                                                point_in_screen);
      }

      did_restore_window_ =
          was_source_maximized_ ||
          (was_source_fullscreen_ && CanRestoreFullscreenWindowDuringDrag());
      if (did_restore_window_) {
        RestoreAttachedWindowForDrag();
      }

      // Drag the window relative to `start_point_in_screen_` to pretend that
      // this was the plan all along.
      const gfx::Vector2d drag_offset =
          start_point_in_screen_ -
          attached_context_->GetWidget()->GetWindowBoundsInScreen().origin();
      return RunMoveLoop(point_in_screen, drag_offset);
    }

    current_state_ = DragState::kDraggingTabs;
    StartDraggingTabsSession(true, point_in_screen);
  }

  return ContinueDragging(point_in_screen);
}

void TabDragController::EndDrag(EndDragReason reason) {
  TRACE_EVENT0("views", "TabDragController::EndDrag");
  presentation_time_recorder_.reset();

  if (!active()) {
    return;
  }

  // Some drags need to react to the model being mutated before the model can
  // change its state.
  if (reason == EndDragReason::kModelAddedTab) {
    // If a group is being dragged, we must place the
    // drag at the current position in the tabstrip or else we will be
    // re-entering into tabstrip mutation code.
    bool group_dragged = false;

    for (const TabDragData& tab_drag_datum : drag_data_.tab_drag_data_) {
      if (tab_drag_datum.view_type == TabSlotView::ViewType::kTabGroupHeader) {
        group_dragged = true;
      }
    }

    if (group_dragged) {
      EndDragImpl(source_context_ == attached_context_ ? EndDragType::kCanceled
                                                       : EndDragType::kNormal);
    }
    return;
  }

  // If we're dragging a window ignore capture lost since it'll ultimately
  // trigger the move loop to end and we'll revert the drag when RunMoveLoop()
  // finishes.
  if (reason == EndDragReason::kCaptureLost &&
      current_state_ == DragState::kDraggingWindow) {
    return;
  }

  // We always lose capture when hiding `attached_context_`, just ignore it.
  if (reason == EndDragReason::kCaptureLost &&
      current_state_ == DragState::kDraggingUsingSystemDnD) {
    return;
  }

  // End the move loop if we're in one. Note that the drag will end (just below)
  // before the move loop actually exits.
  if (current_state_ == DragState::kDraggingWindow && in_move_loop_) {
    VLOG(1) << "EndMoveLoop in EndDrag";
    GetAttachedBrowserWidget()->EndMoveLoop();
  }

  EndDragImpl(reason != EndDragReason::kComplete && source_context_
                  ? EndDragType::kCanceled
                  : EndDragType::kNormal);
}

void TabDragController::SetDragLoopDoneCallbackForTesting(
    base::OnceClosure callback) {
  drag_loop_done_callback_ = std::move(callback);
}

// static
void TabDragController::SetTabDragPointResolver(
    TabDragPointResolver& resolver) {
  g_tab_drag_point_resolver_ = &resolver;
}

void TabDragController::OnWidgetBoundsChanged(views::Widget* widget,
                                              const gfx::Rect& new_bounds) {
  TRACE_EVENT1("views", "TabDragController::OnWidgetBoundsChanged",
               "new_bounds", new_bounds.ToString());
#if defined(USE_AURA)
  aura::Env* env = aura::Env::GetInstance();
  // WidgetBoundsChanged happens as a step of ending a drag, but Drag() doesn't
  // have to be called -- GetCursorScreenPoint() may return an incorrect
  // location in such case and causes a weird effect. See
  // https://crbug.com/914527 for the details.
  if (!env->IsMouseButtonDown() && !env->is_touch_down()) {
    return;
  }
#endif
  // ignore the Liveness; it's fine if Drag destroys `this`.
  std::ignore = Drag(GetCursorScreenPoint());

  // !! N.B. `this` may be deleted here !!
}

void TabDragController::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
}

std::unique_ptr<tabs::TabModel> TabDragController::DetachTabAtForInsertion(
    int drag_idx) {
  CHECK_EQ(current_state_, DragState::kStopped)
      << "Detaching is only supported after the drag is stopped";
  CHECK(attached_context_);

  TabDragData& tab_data = drag_data_.tab_drag_data_[drag_idx];
  // We can't move the tab if `contents` was destroyed during the drag, or if
  // this is a group header.
  CHECK(tab_data.contents);
  tab_data.attached_view->set_detached();
  tab_data.attached_view = nullptr;

  const int from_idx =
      attached_context_->GetTabStripModel()->GetIndexOfWebContents(
          tab_data.contents);
  CHECK_NE(from_idx, TabStripModel::kNoTab);

  base::AutoReset<bool> setter(&is_mutating_, true);
  base::AutoReset<bool> is_removing_last_tab_setter(&is_moving_last_tab_, true);
  std::unique_ptr<tabs::TabModel> detached_tab =
      attached_context_->GetTabStripModel()->DetachTabAtForInsertion(from_idx);
  attached_context_->DraggedTabsDetached();

  return detached_tab;
}

const DragSessionData& TabDragController::GetSessionData() const {
  return drag_data_;
}

void TabDragController::OnSourceTabStripEmpty() {
  // NULL out source_context_ so that we don't attempt to add back to it (in
  // the case of a revert).
  source_context_ = nullptr;
}

void TabDragController::OnActiveStripWebContentsRemoved(
    content::WebContents* contents) {
  // Mark closed tabs as destroyed so we don't try to manipulate them later.
  for (auto& drag_datum : drag_data_.tab_drag_data_) {
    if (drag_datum.contents == contents) {
      drag_datum.contents = nullptr;
      break;
    }
  }
}

void TabDragController::OnActiveStripWebContentsReplaced(
    content::WebContents* previous,
    content::WebContents* next) {
  for (auto& drag_datum : drag_data_.tab_drag_data_) {
    if (drag_datum.contents == previous) {
      drag_datum.contents = next;
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, private:

TabDragController::Liveness TabDragController::SaveFocus() {
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  DCHECK(source_context_);

  old_focused_view_tracker_->SetView(
      source_context_->GetWidget()->GetFocusManager()->GetFocusedView());
  source_context_->GetWidget()->GetFocusManager()->ClearFocus();

  // WARNING: we may have been deleted.
  return ref ? Liveness::kAlive : Liveness::kDeleted;
}

void TabDragController::RestoreFocus() {
  if (attached_context_ != source_context_) {
    if (is_dragging_new_browser_) {
      content::WebContents* active_contents =
          drag_data_.source_dragged_contents();
      if (active_contents) {
        // If the tab is split, make the last active split tab stay active
        // instead of the drag source.
        const tabs::TabInterface* tab =
            tabs::TabInterface::GetFromContents(active_contents);

        if (tab->IsSplit()) {
          TabStripModel* tab_strip_model =
              attached_context_->GetTabStripModel();
          active_contents = tab_strip_model->GetWebContentsAt(
              split_tabs::GetIndexOfLastActiveTab(tab_strip_model,
                                                  tab->GetSplit().value()));
        }

        if (!active_contents->FocusLocationBarByDefault()) {
          active_contents->Focus();
        }
      }
    }
    return;
  }

  views::View* old_focused_view = old_focused_view_tracker_->view();
  if (!old_focused_view || !old_focused_view->GetFocusManager()) {
    return;
  }

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
  if (presentation_time_recorder_) {
    presentation_time_recorder_->RequestNext();
  }
  last_point_in_screen_ = point_in_screen;

  DCHECK(attached_context_);

  TabDragContext* target_context = source_context_;
  if (detach_behavior_ == DetachBehavior::kDetachable) {
    auto [alive, context, drop_target] = GetDragTargetForPoint(point_in_screen);
    if (alive == Liveness::kDeleted) {
      return Liveness::kDeleted;
    }
    target_context = context;
    UpdateDragTarget(drop_target);
    if (current_drag_delegate_) {
      current_drag_delegate_->OnTabDragUpdated(*this, point_in_screen);
    }
  }

  if (target_context != attached_context_) {
    is_dragging_new_browser_ = false;
    did_restore_window_ = false;

    return DragBrowserToNewTabStrip(target_context, point_in_screen);
  }

  if (current_state_ == DragState::kDraggingWindow) {
    bring_to_front_timer_.Start(
        FROM_HERE, base::Milliseconds(750),
        base::BindOnce(&TabDragController::BringWindowUnderPointToFront,
                       base::Unretained(this), point_in_screen));
  }

  if (current_state_ == DragState::kDraggingTabs) {
    dragging_tabs_session_->MoveAttached(point_in_screen);
  }
  return Liveness::kAlive;
}

void TabDragController::UpdateDragTarget(TabDragDelegate* new_target) {
  if (current_drag_delegate_ && current_drag_delegate_ != new_target) {
    current_drag_delegate_->OnTabDragExited();
  }
  current_drag_delegate_ = new_target;
  if (current_drag_delegate_) {
    current_drag_delegate_->OnTabDragEntered();
    drag_delegate_destroyed_subscription_ =
        current_drag_delegate_->RegisterWillDestroyCallback(base::BindOnce(
            &TabDragController::ResetDragTarget, base::Unretained(this)));
  }
}

void TabDragController::ResetDragTarget() {
  current_drag_delegate_ = nullptr;
}

TabDragController::Liveness TabDragController::DragBrowserToNewTabStrip(
    TabDragContext* target_context,
    const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::DragBrowserToNewTabStrip",
               "point_in_screen", point_in_screen.ToString());

  dragging_tabs_session_ = nullptr;

  if (!target_context) {
    return DetachIntoNewBrowserAndRunMoveLoop(point_in_screen);
  }

#if defined(USE_AURA)
  // Only Aura windows are gesture consumers.
  gfx::NativeView attached_native_view =
      GetAttachedBrowserWidget()->GetNativeView();
  GetAttachedBrowserWidget()->GetGestureRecognizer()->TransferEventsTo(
      attached_native_view, target_context->GetWidget()->GetNativeView(),
      ui::TransferTouchesBehavior::kDontCancel);
#endif

  if (current_state_ == DragState::kDraggingWindow) {
    // ReleaseCapture() is going to result in calling back to us (because it
    // results in a move). That'll cause all sorts of problems.  Reset the
    // observer so we don't get notified and process the event.
#if BUILDFLAG(IS_CHROMEOS)
    widget_observation_.Reset();
    move_loop_widget_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)
    views::Widget* browser_widget = GetAttachedBrowserWidget();
    // Disable animations so that we don't see a close animation on aero.
    browser_widget->SetVisibilityChangedAnimationsEnabled(false);
    if (can_release_capture_) {
      browser_widget->ReleaseCapture();
    } else {
      CHECK_EQ(SetCapture(target_context), Liveness::kAlive);
    }

#if !BUILDFLAG(IS_LINUX)
    // EndMoveLoop is going to snap the window back to its original location.
    // Hide it so users don't see this. Hiding a window in Linux aura causes
    // it to lose capture so skip it.
    browser_widget->Hide();
#endif
    // Does not immediately exit the move loop - that only happens when control
    // returns to the event loop. The rest of this method will complete before
    // control returns to RunMoveLoop().
    VLOG(1) << "EndMoveLoop in DragBrowserToNewTabStrip";
    browser_widget->EndMoveLoop();

    // Ideally we would always swap the tabs now, but on non-ash Windows, it
    // seems that running the move loop implicitly activates the window when
    // done, leading to all sorts of flicker. So, on non-ash Windows, instead
    // we process the move after the loop completes. But on ChromeOS Ash, we
    // can do tab swapping now to avoid the tab flashing issue
    // (crbug.com/116329).
    if (can_release_capture_) {
      tab_strip_to_attach_to_after_exit_ = target_context;
      current_state_ = DragState::kWaitingToDragTabs;
    } else {
      // We already transferred ownership of `this` above, before we released
      // capture.
      DetachAndAttachToNewContext(ReleaseCapture::kDontReleaseCapture,
                                  target_context);

      // Enter kWaitingToExitRunLoop until we actually have exited the nested
      // run loop. Otherwise, we might attempt to start another nested run loop,
      // which will CHECK. See https://crbug.com/41493121.
      current_state_ = DragState::kWaitingToExitRunLoop;

      // Move the tabs into position.
      StartDraggingTabsSession(false, point_in_screen);
      attached_context_->GetWidget()->Activate();
    }

    return Liveness::kAlive;
  }

  if (current_state_ == DragState::kDraggingUsingSystemDnD) {
    current_state_ = DragState::kDraggingTabs;
    // Hide the drag image while attached.
    UpdateSystemDnDDragImage(attached_context_, {});
  }

  // In this case we're either:
  // - inserting into a new tabstrip directly from another, without going
  // through any intervening stage of dragging a window. This is possible if the
  // cursor goes over the target tabstrip but isn't far enough from the attached
  // tabstrip to trigger dragging a window;
  // - or the platform does not support RunMoveLoop() and this is the normal
  // behaviour.
  DetachAndAttachToNewContext(ReleaseCapture::kDontReleaseCapture,
                              target_context);

  StartDraggingTabsSession(false, point_in_screen);
  attached_context_->GetWidget()->Activate();
  return Liveness::kAlive;
}

bool TabDragController::ShouldDragWindowUsingSystemDnD() {
  return !GetAttachedBrowserWidget()->IsMoveLoopSupported();
}

void TabDragController::RequestTabThumbnail() {
  WebContents* contents =
      source_context_->GetTabStripModel()->GetActiveWebContents();
  CHECK(contents);
  CHECK(IsDraggingTab(contents));

  content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
  if (rwhv) {
    float scale = rwhv->GetDeviceScaleFactor();
    // Passing an empty source rect means copying the whole surface, an
    // empty target size means no scaling (as we don't know the surface
    // size, it's easier to scale the bitmap to the correct size later).
    rwhv->CopyFromSurface(
        gfx::Rect(), gfx::Size(),
        base::BindOnce(&TabDragController::OnTabThumbnailAvailable,
                       weak_factory_.GetWeakPtr(), scale));
  }
}

void TabDragController::OnTabThumbnailAvailable(
    float window_scale,
    const viz::CopyOutputBitmapWithMetadata& result) {
  const SkBitmap& thumbnail = result.bitmap;
  VLOG(1) << __func__ << " " << thumbnail.width() << "x" << thumbnail.height();
  constexpr size_t kTargetHeightDip = 200;
  constexpr int kRoundedCornerRadius = 4;

  // Exit early if CopyFromSurface() failed, e.g. because the tab was deleted
  // after the drag started.
  if (thumbnail.drawsNothing()) {
    return;
  }

  const float scale = static_cast<float>(kTargetHeightDip) / thumbnail.height();
  drag_image_ = gfx::ImageSkia::CreateFromBitmap(thumbnail, window_scale);
  const gfx::Size target_size =
      gfx::ScaleToCeiledSize(drag_image_.size(), scale);
  drag_image_ = gfx::ImageSkiaOperations::CreateResizedImage(
      drag_image_, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
      target_size);
  drag_image_ = gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
      kRoundedCornerRadius, drag_image_);
  if (current_state_ == DragState::kDraggingUsingSystemDnD) {
    UpdateSystemDnDDragImage(attached_context_, drag_image_);
  }
}

TabDragController::Liveness TabDragController::StartSystemDnDSessionIfNecessary(
    TabDragContext* context,
    gfx::Point point_in_screen) {
  CHECK(ShouldDragWindowUsingSystemDnD());
  CHECK(ui::ResourceBundle::HasSharedInstance());
  current_state_ = DragState::kDraggingUsingSystemDnD;

  if (system_drag_and_drop_session_running_) {
    // Show the drag image again.
    UpdateSystemDnDDragImage(attached_context_, drag_image_);

    return Liveness::kAlive;
  }

  system_drag_and_drop_session_running_ = true;

  if (attached_context_ == source_context_ &&
      drag_data_.num_dragging_tabs() ==
          source_context_->GetTabStripModel()->count()) {
    // When dragging all of a window's tabs, we just hide that window instead of
    // creating a new hidden one. On some platforms (e.g. Wayland) the drag and
    // drop session must be started before hiding the window, so defer until the
    // drag has started.
    drag_started_callback_ = base::BindOnce(
        &TabDragController::HideAttachedContext, weak_factory_.GetWeakPtr());
  }

  auto data_provider = ui::OSExchangeDataProviderFactory::CreateProvider();
  // Set data in a format that is accepted by TabStrip so that a drop can
  // happen.
  base::Pickle pickle;
  data_provider->SetPickledData(
      ui::ClipboardFormatType::CustomPlatformType(ui::kMimeTypeWindowDrag),
      pickle);

  // If the drag image is already available, set it right away. Else we'll get a
  // call to OnTabThumbnailAvailable() and will update it after starting the DnD
  // session.
  if (!drag_image_.isNull()) {
    VLOG(1) << __func__ << " setting drag image";
    data_provider->SetDragImage(
        drag_image_, {drag_image_.width() / 2, drag_image_.height() / 2});
  }

  // Pull into a local to avoid use-after-free if RunShellDrag deletes `this`.
  base::OnceClosure drag_loop_done_callback =
      std::move(drag_loop_done_callback_);

  VLOG(1) << __func__ << " Starting system DnD session";

#if defined(USE_AURA)
  aura::Window* root_window =
      context->GetWidget()->GetNativeWindow()->GetRootWindow();
  aura::client::DragDropClient* client =
      aura::client::GetDragDropClient(root_window);
  if (client) {
    drag_drop_client_observation_.Observe(client);
  }
#endif  // defined(USE_AURA)

  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  context->GetWidget()->RunShellDrag(
      context, std::make_unique<ui::OSExchangeData>(std::move(data_provider)),
      point_in_screen, static_cast<int>(ui::mojom::DragOperation::kMove),
      ui::mojom::DragEventSource::kMouse);

  VLOG(1) << __func__ << " RunShellDrag returned";

  // `RunShellDrag()` may return if we drag all of a window's tabs into another
  // window's tab strip, because that destroys the window. The DnD session is
  // still running and functional, though.

  // If we're still alive and haven't updated our state yet, this means the drag
  // session ended while we were dragging all of the only window's tabs. We need
  // to end the drag session ourselves.
  if (ref && current_state_ == DragState::kDraggingUsingSystemDnD) {
    VLOG(1) << __func__ << " Ending drag";
    EndDrag(EndDragReason::kComplete);
  }

  if (drag_loop_done_callback) {
    std::move(drag_loop_done_callback).Run();
  }

  return ref ? Liveness::kAlive : Liveness::kDeleted;
}

void TabDragController::HideAttachedContext() {
  CHECK_EQ(current_state_, DragState::kDraggingUsingSystemDnD);
  CHECK(GetAttachedBrowserWidget()->IsVisible());

  // See the comment in StartSystemDnDSessionIfNecessary() where we start the
  // drag session.
  GetAttachedBrowserWidget()->Hide();

  // Stop observing, as we're only interested in knowing when the drag started.
#if defined(USE_AURA)
  drag_drop_client_observation_.Reset();
#endif  // defined(USE_AURA)
}

void TabDragController::DetachAndAttachToNewContext(
    ReleaseCapture release_capture,
    TabDragContext* target_context) {
  auto [me, owned_tabs] = Detach(release_capture);
  AttachToNewContext(target_context, std::move(me), std::move(owned_tabs));
}

std::tuple<TabDragController::Liveness, TabDragContext*, TabDragDelegate*>
TabDragController::GetDragTargetForPoint(gfx::Point point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::GetTargetTabStripForPoint",
               "point_in_screen", point_in_screen.ToString());

  gfx::NativeWindow local_window;
  const Liveness state = GetLocalProcessWindow(
      point_in_screen, current_state_ == DragState::kDraggingWindow,
      &local_window);
  if (state == Liveness::kDeleted) {
    return std::tuple(Liveness::kDeleted, nullptr, nullptr);
  }

  if (local_window && CanAttachTo(local_window)) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForNativeWindow(local_window);
    TabDragContext* destination_tab_strip =
        browser_view->tab_strip_view()->GetDragContext();
    if (destination_tab_strip) {
      if (DoesTabStripContain(destination_tab_strip, point_in_screen)) {
        return std::tuple(Liveness::kAlive, destination_tab_strip, nullptr);
      } else if (TabDragDelegate* candidate =
                     GetTabDragPointResolver()->GetDragTarget(
                         *browser_view, point_in_screen)) {
        return std::tuple(Liveness::kAlive,
                          current_state_ == DragState::kDraggingWindow
                              ? attached_context_.get()
                              : nullptr,
                          candidate);
      }
    }
  }

  return std::tuple(Liveness::kAlive,
                    current_state_ == DragState::kDraggingWindow
                        ? attached_context_.get()
                        : nullptr,
                    nullptr);
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

void TabDragController::StartDrag() {
  // `source_context_` already owns `this` (it created us, even), so no need
  // to hand off ownership.
  CHECK_EQ(source_context_->GetDragController(), this);
  CHECK(!current_drag_delegate_);
  attached_context_ = source_context_;

  AttachImpl();

  // Request a thumbnail to use as drag image if we'll use fallback tab
  // dragging.
  if (ShouldDragWindowUsingSystemDnD()) {
    RequestTabThumbnail();
  }
}

void TabDragController::AttachToNewContext(
    TabDragContext* attached_context,
    std::unique_ptr<TabDragController> controller,
    std::vector<std::variant<std::unique_ptr<DetachedTab>,
                             std::unique_ptr<DetachedTabCollection>>>
        owned_tabs_and_collections) {
  // We should already have detached by the time we get here.
  CHECK(!attached_context_);
  attached_context_ = attached_context;

  // `this` is owned by `controller`, and not by `attached_context`.
  CHECK_EQ(this, controller.get());
  CHECK(!attached_context_->GetDragController());

  // Transitioning from detached to attached to a new context. Add tabs to
  // the new model.
  CHECK(GetViewsMatchingDraggedContents(attached_context_).empty());

  selection_model_before_attach_ =
      attached_context_->GetTabStripModel()->selection_model();

  // Insert at any valid index in the tabstrip. We'll fix up the insertion
  // index in MoveAttached() later, if we're transitioning to kDraggingTabs;
  // if we're transitioning to kDraggingWindow this is the correct index, 0.
  size_t index = attached_context_->GetPinnedTabCount();

  base::AutoReset<bool> setter(&is_mutating_, true);

  const auto update_sad_tab = base::BindRepeating(
      [](TabStripModel* model, size_t sad_index) {
        // If a sad tab is showing, the SadTabView needs to be updated.
        SadTabHelper* const sad_tab_helper =
            SadTabHelper::FromWebContents(model->GetWebContentsAt(sad_index));
        if (sad_tab_helper) {
          sad_tab_helper->ReinstallInWebView();
        }
      },
      attached_context_->GetTabStripModel());

  for (auto& tab_or_collection : owned_tabs_and_collections) {
    if (auto* tab =
            std::get_if<std::unique_ptr<DetachedTab>>(&tab_or_collection)) {
      const WebContents* web_contents = tab->get()->tab->GetContents();
      // If it's a tab - we add it to the tabstrip.
      int add_types = AddTabTypes::ADD_NONE;
      TabDragData& tab_data = *std::find_if(
          drag_data_.tab_drag_data_.begin(), drag_data_.tab_drag_data_.end(),
          [web_contents](TabDragData& tab_data) {
            return web_contents == tab_data.contents;
          });
      if (tab_data.pinned) {
        add_types |= AddTabTypes::ADD_PINNED;
      }

      const size_t inserted_index =
          attached_context_->GetTabStripModel()->InsertDetachedTabAt(
              index, std::move(tab->get()->tab), add_types);
      CHECK_EQ(inserted_index, index);
      update_sad_tab.Run(index);
      index++;
    } else {
      gfx::Range collection_indices;
      auto* detached_tab_collection =
          std::get_if<std::unique_ptr<DetachedTabCollection>>(
              &tab_or_collection);

      const bool pinned = detached_tab_collection->get()->pinned_;

      if (std::holds_alternative<std::unique_ptr<tabs::TabGroupTabCollection>>(
              detached_tab_collection->get()->collection_)) {
        collection_indices =
            attached_context_->GetTabStripModel()->InsertDetachedTabGroupAt(
                std::move(*detached_tab_collection), index);
      } else {
        collection_indices =
            attached_context_->GetTabStripModel()->InsertDetachedSplitTabAt(
                std::move(*detached_tab_collection), index, pinned);
      }

      CHECK_EQ(collection_indices.start(), index);
      index += collection_indices.length();

      for (size_t sad_index = collection_indices.start();
           sad_index < collection_indices.end(); sad_index++) {
        update_sad_tab.Run(sad_index);
      }
    }
  }

  // If we're dragging a saved group, resume tracking now that the group is
  // re-attached.
  MaybeResumeTrackingSavedTabGroup();

  AttachImpl();

  attached_context_->OwnDragController(std::move(controller));
}

void TabDragController::AttachImpl() {
  const std::vector<TabSlotView*> views =
      GetViewsMatchingDraggedContents(attached_context_);

  DCHECK_EQ(views.size(), drag_data_.tab_drag_data_.size());
  for (size_t i = 0; i < drag_data_.tab_drag_data_.size(); ++i) {
    drag_data_.tab_drag_data_[i].attached_view = views[i];
  }

  ResetSelection(attached_context_->GetTabStripModel());

  // This should be called after ResetSelection() in order to generate
  // bounds correctly. http://crbug.com/836004
  attached_context_->StartedDragging(views);

  // Make sure the window has capture. This is important so that if activation
  // changes the drag isn't prematurely canceled.
  CHECK_EQ(SetCapture(attached_context_), Liveness::kAlive);

  attached_context_tabs_closed_tracker_ =
      std::make_unique<DraggedTabsClosedTracker>(
          attached_context_->GetTabStripModel(), this);
}

std::tuple<std::unique_ptr<TabDragController>,
           std::vector<std::variant<std::unique_ptr<DetachedTab>,
                                    std::unique_ptr<DetachedTabCollection>>>>
TabDragController::Detach(ReleaseCapture release_capture) {
  TRACE_EVENT1("views", "TabDragController::Detach", "release_capture",
               release_capture);

  attached_context_tabs_closed_tracker_.reset();

  // Detaching may trigger the Widget bounds to change. Such bounds changes
  // should be ignored as they may lead to reentrancy and bad things happening.
  widget_observation_.Reset();

  // Release ownership of the drag controller and mouse capture. When we
  // reattach ownership is transferred.
  std::unique_ptr<TabDragController> me =
      attached_context_->ReleaseDragController();
  DCHECK_EQ(me.get(), this);

  if (release_capture == ReleaseCapture::kReleaseCapture) {
    attached_context_->GetWidget()->ReleaseCapture();
  }

  TabStripModel* attached_model = attached_context_->GetTabStripModel();

  // If we're dragging a saved tab group, suspend tracking between Detach and
  // Attach. Otherwise, the group will get emptied out as we close all the tabs.
  MaybePauseTrackingSavedTabGroup();

  for (TabDragData& tab_drag_datum : drag_data_.tab_drag_data_) {
    // Marking the view as detached tells the TabStrip to not animate its
    // closure, as it's actually being moved.
    // TODO(tbergquist): Is this the right path for this bit to take? Would it
    // make more sense for the model notification to have this information, and
    // for the tabstrip to use that instead?
    tab_drag_datum.attached_view->set_detached();
    // Detaching may end up deleting the view, drop references to it.
    tab_drag_datum.attached_view = nullptr;
  }

  std::vector<int> dragged_indices;
  for (int dragged_index :
       attached_model->selection_model().selected_indices()) {
    dragged_indices.push_back(dragged_index);
  }
  const std::vector<tab_groups::TabGroupId> groups_to_move =
      attached_model->GetGroupsDestroyedFromRemovingIndices(dragged_indices);

  std::vector<std::variant<std::unique_ptr<DetachedTab>,
                           std::unique_ptr<DetachedTabCollection>>>
      owned_tabs_and_collections =
          attached_model->DetachTabsAndCollectionsForInsertion(dragged_indices);

  // If we've removed the last Tab from the TabDragContext, hide the
  // frame now.
  if (!attached_model->empty()) {
    if (!selection_model_before_attach_.empty() &&
        selection_model_before_attach_.active().has_value() &&
        selection_model_before_attach_.active().value() <
            static_cast<size_t>(attached_model->count())) {
      // Restore the selection.
      UpdateSelectionModel(attached_model, selection_model_before_attach_);
    } else if (attached_context_ == source_context_ &&
               !initial_selection_model_.empty()) {
      RestoreInitialSelection();
    }
  }

  attached_context_->DraggedTabsDetached();
  attached_context_ = nullptr;

  return std::make_tuple(std::move(me), std::move(owned_tabs_and_collections));
}

TabDragController::Liveness
TabDragController::DetachIntoNewBrowserAndRunMoveLoop(
    gfx::Point point_in_screen) {
  if (attached_context_->GetTabStripModel()->count() ==
      drag_data_.num_dragging_tabs()) {
    // All the tabs in a browser are being dragged but all the tabs weren't
    // initially being dragged. For this to happen the user would have to
    // start dragging a set of tabs, the other tabs close, then detach.

    if (ShouldDragWindowUsingSystemDnD()) {
      return StartSystemDnDSessionIfNecessary(attached_context_,
                                              point_in_screen);
    }

    const bool restore_window =
        attached_context_->GetWidget()->IsMaximized() ||
        (attached_context_->GetWidget()->IsFullscreen() &&
         CanRestoreFullscreenWindowDuringDrag());
    if (attached_context_ == source_context_) {
      did_restore_window_ = restore_window;
    }
    if (restore_window) {
      RestoreAttachedWindowForDrag();
    }

    const gfx::Vector2d drag_offset =
        point_in_screen -
        attached_context_->GetWidget()->GetWindowBoundsInScreen().origin();
    return RunMoveLoop(point_in_screen, drag_offset);
  }

  const int previous_tab_area_width = attached_context_->GetTabDragAreaWidth();
  const gfx::Size new_size = CalculateDraggedWindowSize(attached_context_);
  const int first_tab_leading_x =
      GetTabOffsetForDetachedWindow(point_in_screen);
  const std::vector<gfx::Rect> drag_bounds =
      attached_context_->CalculateBoundsForDraggedViews(
          drag_data_.attached_views());

  Browser* browser = CreateBrowserForDrag(attached_context_, new_size);

  BrowserView* const dragged_browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::Widget* const dragged_widget = dragged_browser_view->GetWidget();

#if defined(USE_AURA)
  // Only Aura windows are gesture consumers.
  {
    auto ref = weak_factory_.GetWeakPtr();
    base::WeakAutoReset<TabDragController, bool> reentrant_destruction_guard(
        ref, &TabDragController::expect_stay_alive_, true);

    views::Widget* const attached_widget = attached_context_->GetWidget();
    // Unlike DragBrowserToNewTabStrip, this does not have to special-handle
    // IsUsingWindowServices(), since DesktopWIndowTreeHostMus takes care of it.
    attached_widget->GetGestureRecognizer()->TransferEventsTo(
        attached_widget->GetNativeView(), dragged_widget->GetNativeView(),
        ui::TransferTouchesBehavior::kDontCancel);

    // If `attached_context_` received a gesture end event, it will have ended
    // the drag, destroying `this`. This shouldn't ever happen (preventing this
    // scenario is why we pass kDontCancel above) - https://crbug.com/1350564.
    CHECK(ref) << "Drag session was ended as part of transferring events to "
                  "the new browser. This should not happen.";
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  dragged_widget->GetNativeWindow()->SetProperty(
      ash::kTabDraggingSourceWindowKey,
      attached_context_->GetWidget()->GetNativeWindow());

  // On ChromeOS, Detach should release capture; `can_release_capture_` is
  // false on ChromeOS because it can cancel touches, but for this cases
  // the touches are already transferred, so releasing is fine. Without
  // releasing, the capture remains and further touch events can be sent to a
  // wrong target.
  const ReleaseCapture release_capture = ReleaseCapture::kReleaseCapture;
#else
  const ReleaseCapture release_capture =
      can_release_capture_ ? ReleaseCapture::kReleaseCapture
                           : ReleaseCapture::kDontReleaseCapture;
#endif
  DetachAndAttachToNewContext(
      release_capture,
      dragged_browser_view->tab_strip_view()->GetDragContext());

  if (ShouldDragWindowUsingSystemDnD()) {
    // Keep the new window hidden and start a system DnD session.
    //
    // We need to pass `source_context_` here, because the new context we're
    // attached to is hidden and thus can't start the drag session.
    return StartSystemDnDSessionIfNecessary(source_context_, point_in_screen);
  }
  AdjustTabBoundsForDrag(previous_tab_area_width, first_tab_leading_x,
                         drag_bounds);

  const gfx::Vector2d drag_offset = CalculateWindowDragOffset();
#if (!BUILDFLAG(IS_MAC))
  // Set the window origin before making it visible, to avoid flicker on
  // Windows. See https://crbug.com/394529650
  dragged_widget->SetBounds(
      gfx::Rect(point_in_screen - drag_offset, dragged_widget->GetSize()));
#else
  const gfx::Size widget_size = dragged_widget->GetSize();
#endif

  dragged_widget->SetVisibilityChangedAnimationsEnabled(false);
  browser->window()->Show();
  dragged_widget->SetVisibilityChangedAnimationsEnabled(true);

#if BUILDFLAG(IS_MAC)
  // Set the window origin after making it visible, to avoid child windows (such
  // as the find bar) being misplaced on Mac. See https://crbug.com/403129048
  dragged_widget->SetBoundsConstrained(
      gfx::Rect(point_in_screen - drag_offset, widget_size));
#endif

  // Activate may trigger a focus loss, destroying us.
  {
    base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
    browser->window()->Activate();
    if (!ref) {
      return Liveness::kDeleted;
    }
  }
  return RunMoveLoop(point_in_screen, drag_offset);
}

TabDragController::Liveness TabDragController::RunMoveLoop(
    gfx::Point point_in_screen,
    gfx::Vector2d drag_offset) {
  CHECK(!ShouldDragWindowUsingSystemDnD());

  move_loop_widget_ = GetAttachedBrowserWidget();
  DCHECK(move_loop_widget_);
  move_loop_widget_->SetBounds(
      gfx::Rect(point_in_screen - drag_offset, move_loop_widget_->GetSize()));

  // RunMoveLoop can be called reentrantly from within another RunMoveLoop,
  // in which case the observation is already established.
  widget_observation_.Reset();
  widget_observation_.Observe(move_loop_widget_.get());
  current_state_ = DragState::kDraggingWindow;
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  if (can_release_capture_) {
    // Running the move loop releases mouse capture, which triggers destroying
    // the drag loop. Release mouse capture now while the DragController is not
    // owned by the TabDragContext.
    std::unique_ptr<TabDragController> me =
        attached_context_->ReleaseDragController();
    DCHECK_EQ(me.get(), this);
    attached_context_->GetWidget()->ReleaseCapture();
    attached_context_->OwnDragController(std::move(me));
  }
  const views::Widget::MoveLoopSource move_loop_source =
      event_source_ == ui::mojom::DragEventSource::kMouse
          ? views::Widget::MoveLoopSource::kMouse
          : views::Widget::MoveLoopSource::kTouch;
  const views::Widget::MoveLoopEscapeBehavior escape_behavior =
      is_dragging_new_browser_
          ? views::Widget::MoveLoopEscapeBehavior::kHide
          : views::Widget::MoveLoopEscapeBehavior::kDontHide;

  // Pull into a local to avoid use-after-free if RunMoveLoop deletes `this`.
  base::OnceClosure drag_loop_done_callback =
      std::move(drag_loop_done_callback_);

  // This code isn't set up to handle nested run loops. Nested run loops may
  // lead to all sorts of interesting crashes, and generally indicate a bug
  // lower in the stack. This is a CHECK() as there may be security
  // implications to attempting a nested run loop.

  CHECK(!in_move_loop_);
  in_move_loop_ = true;
  views::Widget::MoveLoopResult result = move_loop_widget_->RunMoveLoop(
      drag_offset, move_loop_source, escape_behavior);
  // Note: `this` can be deleted here!

  if (drag_loop_done_callback) {
    std::move(drag_loop_done_callback).Run();
  }

  if (!ref) {
    return Liveness::kDeleted;
  }

  in_move_loop_ = false;
  widget_observation_.Reset();
  move_loop_widget_ = nullptr;

  if (current_state_ == DragState::kWaitingToExitRunLoop) {
    current_state_ = DragState::kDraggingTabs;
  } else if (current_state_ == DragState::kWaitingToDragTabs) {
    DCHECK(tab_strip_to_attach_to_after_exit_);
    DetachAndAttachToNewContext(ReleaseCapture::kDontReleaseCapture,
                                tab_strip_to_attach_to_after_exit_);
    current_state_ = DragState::kDraggingTabs;

    // Move the tabs into position.
    StartDraggingTabsSession(false, GetCursorScreenPoint());
    attached_context_->GetWidget()->Activate();
    // Activate may trigger a focus loss, destroying us.
    if (!ref) {
      return Liveness::kDeleted;
    }
    tab_strip_to_attach_to_after_exit_ = nullptr;
  } else if (current_state_ == DragState::kDraggingWindow) {
    EndDrag(result == views::Widget::MoveLoopResult::kCanceled
                ? EndDragReason::kCancel
                : EndDragReason::kComplete);
    return Liveness::kDeleted;
  }

  return Liveness::kAlive;
}

std::vector<TabSlotView*> TabDragController::GetViewsMatchingDraggedContents(
    TabDragContext* context) {
  const TabStripModel* const model = context->GetTabStripModel();
  std::vector<TabSlotView*> views;
  for (const TabDragData& tab_drag_datum : drag_data_.tab_drag_data_) {
    if (tab_drag_datum.view_type == TabSlotView::ViewType::kTab) {
      const int model_index =
          model->GetIndexOfWebContents(tab_drag_datum.contents);
      if (model_index == TabStripModel::kNoTab) {
        return {};
      }
      views.push_back(context->GetTabAt(model_index));
    } else {
      // Return empty vector if the group is not present in the model.
      if (!model->group_model()->ContainsTabGroup(
              tab_drag_datum.tab_group_data->group_id)) {
        return {};
      }

      TabGroupHeader* header =
          context->GetTabGroupHeader(tab_drag_datum.tab_group_data->group_id);
      CHECK(header);
      views.push_back(header);
    }
  }
  return views;
}

void TabDragController::EndDragImpl(EndDragType type) {
  VLOG(1) << __func__ << " type=" << static_cast<int>(type)
          << " state=" << static_cast<int>(current_state_);

  DragState previous_state = current_state_;
  current_state_ = DragState::kStopped;
  attached_context_tabs_closed_tracker_.reset();

  bring_to_front_timer_.Stop();

  // Before clearing the tab drag data check to see if there was a tab that was
  // dragged from out of a tab group into a tab group.
  NotifyEventIfTabAddedToGroup();

  if (type != EndDragType::kTabDestroyed) {
    // We only finish up the drag if we were actually dragging. If start_drag_
    // is false, the user just clicked and released and didn't move the mouse
    // enough to trigger a drag.
    if (previous_state != DragState::kNotStarted) {
      // After the drag ends, sometimes it shouldn't restore the focus, because
      // - Some dragging gesture (like fling down) minimizes the window, but the
      //   window activation cancels minimized status. See
      //   https://crbug.com/902897
      if (!attached_context_->GetWidget()->IsMinimized()) {
        RestoreFocus();
      }

      GetAttachedBrowserWidget()->SetCanAppearInExistingFullscreenSpaces(false);
      if (type == EndDragType::kCanceled) {
        RevertDrag();
      } else {
        if (previous_state == DragState::kDraggingUsingSystemDnD) {
// `views::CancelShellDrag()` is only available on Aura, and on all non-Aura
// platforms `IsMoveLoopSupported()` returns true anyways.
#if defined(USE_AURA)
          // Make sure the drag session ends.
          if (system_drag_and_drop_session_running_) {
            // We need to pass `allow_widget_mismatch=true` here, because
            // without it we're not allowed to cancel drags initiated by a
            // different widget; but the initiating widget (the one belonging to
            // `source_context_`) might have been destroyed during the drag (for
            // example when dragging all tabs of a window into another window).
            gfx::NativeView view = GetAttachedBrowserWidget()->GetNativeView();
            views::CancelShellDrag(view, /*allow_widget_mismatch=*/true);
          }
#endif  // defined(USE_AURA)

          // Make the hidden window containing the dragged tabs visible.
          GetAttachedBrowserWidget()->Show();
        }
        CompleteDrag();
      }
    }
  } else if (drag_data_.tab_drag_data_.size() > 1) {
    initial_selection_model_.Clear();
    if (previous_state != DragState::kNotStarted) {
      RevertDrag();
    }
  }  // else case the only tab we were dragging was deleted. Nothing to do.

  // Clear out drag data so we don't attempt to do anything with it.
  drag_data_.tab_drag_data_.clear();
  if (current_drag_delegate_) {
    current_drag_delegate_->OnTabDragEnded();
    ResetDragTarget();
  }

  TabDragContext* owning_context =
      attached_context_ ? attached_context_.get() : source_context_.get();
  owning_context->DestroyDragController();
}

void TabDragController::RevertDrag() {
  CHECK(attached_context_);
  CHECK(source_context_);

  dragging_tabs_session_ = nullptr;

  // If we're dragging a saved tab group, suspend tracking during the revert.
  // Otherwise, the group will get emptied out as we revert all the tabs.
  MaybePauseTrackingSavedTabGroup();

  base::AutoReset<bool> is_mutating_setter(&is_mutating_, true);
  base::AutoReset<bool> is_removing_last_tab_setter(&is_moving_last_tab_, true);

  if (attached_context_ != source_context_) {
    for (TabDragData& tab_datum : drag_data_.tab_drag_data_) {
      tab_datum.attached_view->set_detached();
      tab_datum.attached_view = nullptr;
    }
  }

  // Revert each tab, split and group. We manually increment `i` because
  // each group or split has multiple entries in `tab_drag_data_`.
  for (size_t i = 0; i < drag_data_.tab_drag_data_.size();) {
    const TabDragData tab_data = drag_data_.tab_drag_data_[i];
    if (tab_data.view_type == TabSlotView::ViewType::kTabGroupHeader) {
      RevertGroupAt(i);
      // Skip all the tabs in the group too.
      i += source_context_->GetTabStripModel()
               ->group_model()
               ->GetTabGroup(tab_data.tab_group_data->group_id)
               ->tab_count() +
           1;
    } else {
      CHECK(tab_data.contents);

      const tabs::TabInterface* tab =
          tabs::TabInterface::GetFromContents(tab_data.contents);

      if (tab->IsSplit()) {
        split_tabs::SplitTabId split_id = tab->GetSplit().value();
        RevertSplitAt(i);
        i += source_context_->GetTabStripModel()
                 ->GetSplitData(split_id)
                 ->ListTabs()
                 .size();
      } else {
        RevertTabAt(i);
        i++;
      }
    }
  }
  MaybeResumeTrackingSavedTabGroup();

  if (did_restore_window_) {
    MaximizeAttachedWindow();
  }
  if (attached_context_ == source_context_) {
    source_context_->StoppedDragging();
  } else {
    attached_context_->DraggedTabsDetached();
  }

  // If tabs were closed during this drag, the initial selection might include
  // indices that are out of bounds for the tabstrip now. Reset the selection to
  // include the still-existing currently dragged WebContentses.
  for (int selection : initial_selection_model_.selected_indices()) {
    if (!source_context_->GetTabStripModel()->ContainsIndex(selection)) {
      initial_selection_model_.Clear();
      break;
    }
  }

  if (initial_selection_model_.empty()) {
    ResetSelection(source_context_->GetTabStripModel());
  } else {
    UpdateSelectionModel(source_context_->GetTabStripModel(),
                         initial_selection_model_);
  }

  source_context_->GetWidget()->Activate();
}

void TabDragController::ResetSelection(TabStripModel* model) {
  DCHECK(model);
  ui::ListSelectionModel selection_model;
  bool has_one_valid_tab = false;
  for (size_t i = 0; i < drag_data_.tab_drag_data_.size(); ++i) {
    // `contents` is NULL if a tab was deleted out from under us.
    if (drag_data_.tab_drag_data_[i].contents) {
      int index =
          model->GetIndexOfWebContents(drag_data_.tab_drag_data_[i].contents);
      DCHECK_GE(index, 0);
      selection_model.AddIndexToSelection(static_cast<size_t>(index));
      // Set this tab as active if:
      // a) we don't have an active tab yet
      // b) this was the source view for the drag
      // c) we're in a header drag, and this tab was active before the drag
      if (!has_one_valid_tab || i == drag_data_.source_view_index_ ||
          (drag_data_.group_drag_data_.has_value() &&
           (drag_data_.group_drag_data_.value().active_tab_index_within_group +
            1) == static_cast<int>(i))) {
        // Reset the active/lead to the first tab. If the source tab is still
        // valid we'll reset these again later on.
        selection_model.set_active(static_cast<size_t>(index));
        selection_model.set_anchor(static_cast<size_t>(index));
        has_one_valid_tab = true;
      }
    }
  }
  if (!has_one_valid_tab) {
    return;
  }

  UpdateSelectionModel(model, selection_model);
}

void TabDragController::RestoreInitialSelection() {
  // First time detaching from the source tabstrip. Reset selection model to
  // initial_selection_model_. Before resetting though we have to remove all
  // the tabs from initial_selection_model_ as it was created with the tabs
  // still there.
  ui::ListSelectionModel selection_model = initial_selection_model_;
  for (const TabDragData& data : base::Reversed(drag_data_.tab_drag_data_)) {
    if (data.source_model_index.has_value()) {
      selection_model.DecrementFrom(data.source_model_index.value());
    }
  }
  // We may have cleared out the selection model. Only reset it if it
  // contains something.
  if (selection_model.empty()) {
    return;
  }

  // Tabs in `source_context_` may have closed since the drag began. In that
  // case, `initial_selection_model_` may include indices that are no longer
  // valid in `source_context_`. Abort restoring the selection if so.
  if (!source_context_->GetTabStripModel()->ContainsIndex(
          *(selection_model.selected_indices().rbegin()))) {
    return;
  }

  // The anchor/active may have been among the tabs that were dragged out. Force
  // the anchor/active to be valid.
  if (!selection_model.anchor().has_value()) {
    selection_model.set_anchor(*selection_model.selected_indices().begin());
  }
  if (!selection_model.active().has_value()) {
    selection_model.set_active(*selection_model.selected_indices().begin());
  }
  UpdateSelectionModel(source_context_->GetTabStripModel(), selection_model);
}

void TabDragController::RevertGroupAt(size_t drag_index) {
  const tab_groups::TabGroupId group_id =
      drag_data_.tab_drag_data_[drag_index].tab_group_data->group_id;
  const TabDragData first_tab_in_group =
      drag_data_.tab_drag_data_[drag_index + 1];
  int target_index = first_tab_in_group.source_model_index.value();
  if (attached_context_ != source_context_) {
    source_context_->GetTabStripModel()->InsertDetachedTabGroupAt(
        attached_context_->GetTabStripModel()->DetachTabGroupForInsertion(
            group_id),
        target_index);
    source_context_->GetTabStripModel()->ChangeTabGroupVisuals(
        group_id, first_tab_in_group.tab_group_data->group_visual_data);
    return;
  }

  const int index =
      attached_context_->GetTabStripModel()->GetIndexOfWebContents(
          first_tab_in_group.contents);
  CHECK_NE(index, TabStripModel::kNoTab);

  if (target_index > index) {
    for (size_t i = drag_index + 1; i < drag_data_.tab_drag_data_.size(); ++i) {
      const TabDragData other_tab = drag_data_.tab_drag_data_[i];
      // Ignore group headers, they don't have model indices to skip over.
      if (other_tab.view_type != TabSlotView::ViewType::kTab) {
        continue;
      }
      // Ignore other tabs in this group, they will get moved along with us
      // so we won't skip over them.
      if (other_tab.tab_group_data.has_value() &&
          other_tab.tab_group_data->group_id == group_id) {
        continue;
      }

      ++target_index;
    }
  }

  source_context_->GetTabStripModel()->MoveGroupTo(
      first_tab_in_group.tab_group_data->group_id, target_index);
}

void TabDragController::RevertSplitAt(size_t drag_index) {
  CHECK_NE(current_state_, DragState::kNotStarted);
  CHECK(attached_context_);
  CHECK(source_context_);
  // We can't revert if `contents` was destroyed during the drag, or if this is
  // a group header.
  CHECK(drag_data_.tab_drag_data_[drag_index].contents);

  const TabDragData tab_data = drag_data_.tab_drag_data_[drag_index];
  const tabs::TabInterface* tab =
      tabs::TabInterface::GetFromContents(tab_data.contents);
  split_tabs::SplitTabId split_id = tab->GetSplit().value();

  // The split will be reverted into its original group.
  const std::optional<tab_groups::TabGroupId> existing_group =
      tab_data.tab_group_data.has_value()
          ? std::make_optional(tab_data.tab_group_data->group_id)
          : std::nullopt;

  const int from_index =
      attached_context_->GetTabStripModel()->GetIndexOfWebContents(
          tab_data.contents);
  CHECK_NE(from_index, TabStripModel::kNoTab);
  int target_index = tab_data.source_model_index.value();

  if (attached_context_ != source_context_) {
    // The Split was inserted into another TabDragContext. We need to
    // put it back into the original one.
    std::unique_ptr<DetachedTabCollection> detached_split =
        attached_context_->GetTabStripModel()->DetachSplitTabForInsertion(
            split_id);
    source_context_->GetTabStripModel()->InsertDetachedSplitTabAt(
        std::move(detached_split), target_index, tab_data.pinned,
        existing_group);
  } else {
    if (target_index > from_index) {
      for (size_t i = drag_index + 1; i < drag_data_.tab_drag_data_.size();
           ++i) {
        const TabDragData other_tab = drag_data_.tab_drag_data_[i];

        // Ignore group headers, they don't have model indices to skip over.
        if (other_tab.view_type != TabSlotView::ViewType::kTab) {
          continue;
        }

        tabs::TabInterface* other_tab_interface =
            tabs::TabInterface::GetFromContents(other_tab.contents);

        CHECK(other_tab_interface);

        // Ignore other tabs in this split, they will get moved along with us
        // so we won't skip over them.
        if (other_tab_interface->GetSplit() == split_id) {
          continue;
        }

        ++target_index;
      }
    }

    source_context_->GetTabStripModel()->MoveSplitTo(
        split_id, target_index, tab_data.pinned, existing_group);
  }
}

void TabDragController::RevertTabAt(size_t drag_index) {
  CHECK_NE(current_state_, DragState::kNotStarted);
  CHECK(attached_context_);
  CHECK(source_context_);
  // We can't revert if `contents` was destroyed during the drag, or if this is
  // a group header.
  CHECK(drag_data_.tab_drag_data_[drag_index].contents);

  const TabDragData tab_data = drag_data_.tab_drag_data_[drag_index];

  // The tab will be reverted into its original group.
  const std::optional<tab_groups::TabGroupId> existing_group =
      tab_data.tab_group_data.has_value()
          ? std::make_optional(tab_data.tab_group_data->group_id)
          : std::nullopt;

  const int from_index =
      attached_context_->GetTabStripModel()->GetIndexOfWebContents(
          tab_data.contents);
  CHECK_NE(from_index, TabStripModel::kNoTab);
  int target_index = tab_data.source_model_index.value();

  if (attached_context_ != source_context_) {
    // The Tab was inserted into another TabDragContext. We need to
    // put it back into the original one.
    std::unique_ptr<tabs::TabModel> detached_tab =
        attached_context_->GetTabStripModel()->DetachTabAtForInsertion(
            from_index);
    source_context_->GetTabStripModel()->InsertDetachedTabAt(
        target_index, std::move(detached_tab),
        (tab_data.pinned ? AddTabTypes::ADD_PINNED : 0), existing_group);
  } else {
    // The Tab was moved within the TabDragContext where the drag
    // was initiated. Move it back to the starting location.

    // If the target index is to the right, then other unreverted tabs are
    // occupying indices between this tab and the target index. Those
    // unreverted tabs will later be reverted to the right of the target
    // index, so we skip those indices.
    if (target_index > from_index) {
      for (size_t i = drag_index + 1; i < drag_data_.tab_drag_data_.size();
           ++i) {
        if (drag_data_.tab_drag_data_[i].contents) {
          ++target_index;
        }
      }
    }

    source_context_->GetTabStripModel()->MoveWebContentsAt(
        from_index, target_index, false, existing_group);
  }
}

void TabDragController::CompleteDrag() {
  CHECK_NE(current_state_, DragState::kNotStarted);
  CHECK(attached_context_);

  if (current_drag_delegate_ && current_drag_delegate_->CanDropTab()) {
    current_drag_delegate_->HandleTabDrop(*this);
    attached_context_->StoppedDragging();

    // The delegate is expected to handle all tab dragging finalization, and
    // therefore we return here.
    // The logic below is specific for dragging to a tabstrip, most of which
    // should be moved to tabstrip's `TabDragDelegate` implementation. Some
    // functionality, such as restoring tab model selection, may be shared as
    // part of the `TabDragDelegate::DragController` interface.
    return;
  }

  if (is_dragging_new_browser_ || did_restore_window_) {
    if (IsSnapped(attached_context_)) {
      was_source_maximized_ = false;
      was_source_fullscreen_ = false;
    }

    // If source window was maximized - maximize the new window as well.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_MAC)
    // Keeping maximized state breaks snap to Grid on Windows when dragging
    // tabs from maximized windows. TODO:(crbug.com/727051) Explore doing this
    // for other desktop OS's. kMaximizedStateRetainedOnTabDrag in
    // tab_drag_controller_interactive_uitest.cc will need to be initialized
    // to false on each desktop OS that changes this behavior.
    // macOS opts out since this maps maximize to fullscreen, which can
    // violate user expectations and interacts poorly with some window
    // management actions.
    if (was_source_maximized_ || was_source_fullscreen_) {
      MaximizeAttachedWindow();
    }
#endif  // !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_MAC)
  }
  attached_context_->StoppedDragging();

  // Tabbed PWAs with a home tab should have a home tab in every window.
  // This means when dragging tabs out to create a new window, a home tab
  // needs to be added.
  if (is_dragging_new_browser_) {
    Browser* new_browser =
        BrowserView::GetBrowserViewForNativeWindow(
            attached_context_->GetWidget()->GetNativeWindow())
            ->browser();

    if (new_browser->app_controller() &&
        new_browser->app_controller()->has_tab_strip()) {
      web_app::MaybeAddPinnedHomeTab(new_browser,
                                     new_browser->app_controller()->app_id());
    }
  }

  if (drag_data_.group_drag_data_.has_value()) {
    // Manually reset the selection to just the active tab in the group.
    // For multi tab select keep the selection model as the user used it as the
    // original selection.
    TabStripModel* model = attached_context_
                               ? attached_context_->GetTabStripModel()
                               : source_context_->GetTabStripModel();
    ui::ListSelectionModel selection;
    // Offset by 1 to account for the group header.
    const int drag_data_index =
        1 + drag_data_.group_drag_data_.value().active_tab_index_within_group;
    const int index = model->GetIndexOfWebContents(
        drag_data_.tab_drag_data_[drag_data_index].contents);

    // The tabs in the group may have been closed during the drag.
    if (index != TabStripModel::kNoTab) {
      DCHECK_GE(index, 0);
      selection.AddIndexToSelection(static_cast<size_t>(index));
      selection.set_active(static_cast<size_t>(index));
      selection.set_anchor(static_cast<size_t>(index));
      UpdateSelectionModel(model, selection);
    }
  }

  if (source_context_ == attached_context_) {
    LogTabStripOrganizationUKM(
        attached_context_->GetTabStripModel(),
        SuggestedTabStripOrganizationReason::kDraggedWithinSameTabstrip);
  }
}

void TabDragController::MaximizeAttachedWindow() {
  GetAttachedBrowserWidget()->Maximize();
#if BUILDFLAG(IS_MAC)
  if (was_source_fullscreen_) {
    GetAttachedBrowserWidget()->SetFullscreen(true);
  }
#endif
#if BUILDFLAG(IS_CHROMEOS)
  if (was_source_fullscreen_) {
    // In fullscreen mode it is only possible to get here if the source
    // was in "immersive fullscreen" mode, so toggle it back on.
    BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
        GetAttachedBrowserWidget()->GetNativeWindow());
    DCHECK(browser_view);
    if (!browser_view->IsFullscreen()) {
      chrome::ToggleFullscreenMode(browser_view->browser(),
                                   /*user_initiated=*/false);
    }
  }
#endif
}

void TabDragController::BringWindowUnderPointToFront(
    const gfx::Point& point_in_screen) {
  gfx::NativeWindow native_window;
  if (GetLocalProcessWindow(point_in_screen, true, &native_window) ==
      Liveness::kDeleted) {
    return;
  }

  if (!native_window) {
    return;
  }

  // Only bring browser windows to front - only windows with a
  // TabDragContext can be tab drag targets.
  if (!CanAttachTo(native_window)) {
    return;
  }

  views::Widget* widget_window =
      views::Widget::GetWidgetForNativeWindow(native_window);
  if (!widget_window) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
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
  for (aura::Window* window :
       base::Reversed(browser_window->parent()->children())) {
    // If the iteration reached the recipient browser window then it is
    // already topmost and it is safe to return with no stacking change.
    if (window == browser_window) {
      return;
    }
    if (window->GetType() != aura::client::WINDOW_TYPE_POPUP) {
      widget_window->StackAbove(window);
      break;
    }
  }
#else
  widget_window->StackAtTop();
#endif

  // The previous call made the window appear on top of the dragged window,
  // move the dragged window to the front.
  if (current_state_ == DragState::kDraggingWindow) {
    attached_context_->GetWidget()->StackAtTop();
  }
}

TabDragController::Liveness TabDragController::SetCapture(
    TabDragContext* context) {
  auto ref = weak_factory_.GetWeakPtr();
  context->GetWidget()->SetCapture(context);
  return ref ? Liveness::kAlive : Liveness::kDeleted;
}

bool TabDragController::IsDraggingTab(content::WebContents* contents) const {
  for (auto& drag_data : drag_data_.tab_drag_data_) {
    if (drag_data.contents == contents) {
      return true;
    }
  }
  return false;
}

views::Widget* TabDragController::GetAttachedBrowserWidget() {
  return attached_context_->GetWidget();
}

void TabDragController::RestoreAttachedWindowForDrag() {
  const gfx::Size restored_size = CalculateDraggedWindowSize(attached_context_);

  views::Widget* widget = attached_context_->GetWidget();
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->Restore();
  widget->SetVisibilityChangedAnimationsEnabled(true);

  widget->SetSize(restored_size);
}

gfx::Size TabDragController::CalculateDraggedWindowSize(
    TabDragContext* source) {
  // To support immersive fullscreen on macOS get the top level widget, which
  // will be the browser widget. In immersive fullscreen there are other
  // intermediate widgets that host the toolbar and tab strip, getting their
  // restored bounds is not helpful. Getting the top level widget in the
  // non-immersive fullscreen case is a essentially a NOP, so use it
  // unconditionally here for all platforms.
  gfx::Size new_size(
      source->GetWidget()->GetTopLevelWidget()->GetRestoredBounds().size());

  // Limit the window size to the current display's size, less some insets.
  const gfx::Size work_area =
      display::Screen::Get()
          ->GetDisplayNearestPoint(last_point_in_screen_)
          .work_area()
          .size();
  if (new_size.width() >= work_area.width() &&
      new_size.height() >= work_area.height()) {
    new_size = work_area;
    new_size.Enlarge(-2 * kMaximizedWindowInset, -2 * kMaximizedWindowInset);
  }

  if (source->GetWidget()->IsMaximized()) {
    // If the restore bounds is really small, we don't want to honor it
    // (dragging a really small window looks wrong), instead make sure the new
    // window is at least 50% the size of the old.
    const gfx::Size maximized_size(
        source->GetWidget()->GetWindowBoundsInScreen().size());
    new_size.set_width(std::max(maximized_size.width() / 2, new_size.width()));
    new_size.set_height(
        std::max(maximized_size.height() / 2, new_size.height()));
  }

  return new_size;
}

int TabDragController::GetTabOffsetForDetachedWindow(
    gfx::Point point_in_screen) {
  DCHECK(attached_context_);
  const gfx::Point attached_point =
      views::View::ConvertPointFromScreen(attached_context_, point_in_screen);
  if (attached_point.x() < attached_context_->TabDragAreaBeginX()) {
    // Detaching to the left; tabs should be at the beginning of the window.
    return 0;
  }
  if (attached_point.x() >= attached_context_->TabDragAreaEndX()) {
    // Detaching to the right; tabs should be at the beginning of the window.
    return 0;
  }

  // Detaching above or below; tabs should keep their current offset.
  return drag_data_.tab_drag_data_[0].attached_view->bounds().x();
}

void TabDragController::AdjustTabBoundsForDrag(
    int previous_tab_area_width,
    int first_tab_leading_x,
    std::vector<gfx::Rect> drag_bounds) {
  CHECK(!ShouldDragWindowUsingSystemDnD());

  attached_context_->ForceLayout();
  const int current_tab_area_width = attached_context_->GetTabDragAreaWidth();

  // If the new tabstrip region is smaller than the old, resize and reposition
  // the tabs to provide a sense of continuity.
  if (current_tab_area_width < previous_tab_area_width) {
    // TODO(crbug.com/40839358): Fix the case where the source window
    // spans two monitors horizontally, and IsRTL is true.

    // `leading_ratio` is the proportion of the previous tab area width which is
    // ahead of the first dragged tab's previous position.
    const float leading_ratio =
        first_tab_leading_x / static_cast<float>(previous_tab_area_width);

    // If the tabs can fit within the new tab area with room to spare, align
    // them within it so the leading tab is in the same position as it was in
    // the previous tab area, proportionally speaking.
    if (drag_bounds.back().right() < current_tab_area_width) {
      // The tabs must stay within the tabstrip.
      const int maximum_tab_x =
          current_tab_area_width -
          (drag_bounds.back().right() - drag_bounds.front().x());
      const int leading_tab_x =
          std::min(static_cast<int>(leading_ratio * current_tab_area_width),
                   maximum_tab_x);
      OffsetX(leading_tab_x, &drag_bounds);
    }
  } else {
    OffsetX(first_tab_leading_x, &drag_bounds);
  }
  attached_context_->SetBoundsForDrag(drag_data_.attached_views(), drag_bounds);
}

std::optional<webapps::AppId> TabDragController::GetControllingAppForDrag(
    Browser* browser) {
  content::WebContents* active_contents = drag_data_.source_dragged_contents();
  if (!base::FeatureList::IsEnabled(
          features::kTearOffWebAppTabOpensWebAppWindow) ||
      drag_data_.tab_drag_data_.size() != 1 || !active_contents) {
    return std::nullopt;
  }
  const web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(browser->profile());
  const base::flat_map<webapps::AppId, std::string> all_controlling_apps =
      provider->registrar_unsafe().GetAllAppsControllingUrl(
          active_contents->GetLastCommittedURL());
  if (all_controlling_apps.size() != 1) {
    return std::nullopt;
  }

  return all_controlling_apps.begin()->first;
}

Browser* TabDragController::CreateBrowserForDrag(TabDragContext* source,
                                                 gfx::Size initial_size) {
  source->GetWidget()
      ->GetCompositor()
      ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
          [](base::TimeTicks now,
             const viz::FrameTimingDetails& frame_timing_details) {
            base::TimeTicks presentation_timestamp =
                frame_timing_details.presentation_feedback.timestamp;
            UmaHistogramTimes(kDragToNewBrowserPresentationTimeHistogram,
                              presentation_timestamp - now);
          },
          base::TimeTicks::Now()));

  // Find if there's a controlling app, and thus we should open an app window.
  Browser* from_browser = BrowserView::GetBrowserViewForNativeWindow(
                              GetAttachedBrowserWidget()->GetNativeWindow())
                              ->browser();

  const std::optional<webapps::AppId> controlling_app =
      GetControllingAppForDrag(from_browser);
  const bool open_as_web_app = controlling_app.has_value();

  Browser::CreateParams create_params =
      open_as_web_app
          ? Browser::CreateParams::CreateForApp(
                web_app::GenerateApplicationNameFromAppId(
                    controlling_app.value()),
                /* trusted_source=*/true, gfx::Rect(), from_browser->profile(),
                /* user_gesture=*/true)
          : from_browser->create_params();

  // Web app windows have their own initial size independent of the source
  // browser window.
  if (!open_as_web_app) {
    create_params.initial_bounds = gfx::Rect(initial_size);
  }
  create_params.user_gesture = true;
  create_params.in_tab_dragging = true;
#if BUILDFLAG(IS_CHROMEOS)
  // Do not copy attached window's restore id as this will cause Full Restore to
  // restore the newly created browser using the original browser's stored data.
  // See crbug.com/1208923 and crbug.com/1333562 for details.
  create_params.restore_id = Browser::kDefaultRestoreId;

  // Open the window in the same display.
  display::Display display = display::Screen::Get()->GetDisplayNearestWindow(
      source->GetWidget()->GetNativeWindow());
  create_params.display_id = display.id();
#endif
  // Do not copy attached window's show state as the attached window might be a
  // maximized or fullscreen window and we do not want the newly created browser
  // window is a maximized or fullscreen window since it will prevent window
  // moving/resizing on Chrome OS. See crbug.com/1023871 for details.
  create_params.initial_show_state = ui::mojom::WindowShowState::kDefault;

  // Don't copy the initial workspace since the *current* workspace might be
  // different and copying the workspace will move the tab to the initial one.
  create_params.initial_workspace = std::string();

  // Don't copy the window name - the user's deliberately creating a new window,
  // which should default to its own auto-generated name, not the same name as
  // the previous window.
  create_params.user_title = std::string();

  Browser* browser = Browser::Create(create_params);
  is_dragging_new_browser_ = true;
  BrowserView::GetBrowserViewForBrowser(browser)
      ->GetWidget()
      ->SetCanAppearInExistingFullscreenSpaces(true);

#if !BUILDFLAG(IS_CHROMEOS)
  // If the window is created maximized then the bounds we supplied are ignored.
  // We need to reset them again so they are honored. On ChromeOS, this is
  // handled in NativeWidgetAura.
  if (!open_as_web_app) {
    browser->window()->SetBounds(gfx::Rect(initial_size));
  }
#endif

  return browser;
}

gfx::Point TabDragController::GetCursorScreenPoint() {
#if BUILDFLAG(IS_CHROMEOS)
  views::Widget* widget = GetAttachedBrowserWidget();
  DCHECK(widget);
  aura::Window* widget_window = widget->GetNativeWindow();
  DCHECK(widget_window->GetRootWindow());
  return aura::Env::GetInstance()->GetLastPointerPoint(
      event_source_, widget_window, /*fallback=*/last_point_in_screen_);
#else
  return display::Screen::Get()->GetCursorScreenPoint();
#endif
}

gfx::Vector2d TabDragController::CalculateWindowDragOffset() {
  const gfx::Rect source_tab_bounds =
      drag_data_.attached_views()[drag_data_.source_view_index_]->bounds();
  const int cursor_offset_within_tab =
      base::ClampRound(source_tab_bounds.width() * offset_to_width_ratio_);
  gfx::Point desired_cursor_pos_in_widget(
      attached_context_->GetMirroredXInView(source_tab_bounds.x() +
                                            cursor_offset_within_tab),
      source_tab_bounds.height() / 2);
  views::View::ConvertPointToWidget(attached_context_,
                                    &desired_cursor_pos_in_widget);
  return desired_cursor_pos_in_widget.OffsetFromOrigin();
}

TabDragController::Liveness TabDragController::GetLocalProcessWindow(
    const gfx::Point& screen_point,
    bool exclude_dragged_view,
    gfx::NativeWindow* window) {
  std::set<gfx::NativeWindow> exclude;
  if (exclude_dragged_view) {
    gfx::NativeWindow dragged_window =
        attached_context_->GetWidget()->GetNativeWindow();
    if (dragged_window) {
      exclude.insert(dragged_window);
    }
  }

#if BUILDFLAG(IS_LINUX)
  // Exclude windows which are pending deletion via Browser::TabStripEmpty().
  // These windows can be returned in the Linux Aura port because the browser
  // window which was used for dragging is not hidden once all of its tabs are
  // attached to another browser window in DragBrowserToNewTabStrip().
  // TODO(pkotwicz): Fix this properly (crbug.com/358482)
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&exclude](BrowserWindowInterface* browser) {
        if (browser->GetTabStripModel()->empty()) {
          exclude.insert(browser->GetWindow()->GetNativeWindow());
        }
        return true;
      });
#endif
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  *window = window_finder_->GetLocalProcessWindowAtPoint(screen_point, exclude);
  return ref ? Liveness::kAlive : Liveness::kDeleted;
}

bool TabDragController::CanAttachTo(gfx::NativeWindow window) {
  if (!window) {
    return false;
  }

  if (window == GetAttachedBrowserWidget()->GetNativeWindow()) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/447246798): Don't allow dragging into an overview item as
  // the implementation is buggy. Triggering this appears to require drag by
  // touch, as drag by click causes the overview session to end immediately.
  if (window->GetProperty(chromeos::kIsShowingInOverviewKey)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Return false if `other_browser_view` is null or already closed. The latter
  // check is required since the widget may still alive on asynchronous
  // platforms such as Mac.
  BrowserView* other_browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  if (!other_browser_view || other_browser_view->GetWidget()->IsClosed()) {
    return false;
  }
  Browser* other_browser = other_browser_view->browser();

  // Do not allow dragging into a window with a modal dialog, it causes a
  // weird behavior.  See crbug.com/336691
#if defined(USE_AURA)
  if (wm::GetModalTransient(window)) {
    return false;
  }
#else
  TabStripModel* model = other_browser->tab_strip_model();
  DCHECK(model);

  const int active_index = model->active_index();

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40890295): Remove DumpWithoutCrashing() if
  // Widget::IsClosed() check above works.
  if (!model->ContainsIndex(active_index)) {
    if (active_index == TabStripModel::kNoTab) {
      LOG(ERROR) << "TabStripModel of the browser tyring to attach to has no "
                    "active tab.";
    } else {
      LOG(ERROR)
          << "TabStripModel of the browser trying to attach to has invalid "
          << "active index: " << active_index;
    }

    // Avoid dumping too many times not to impact the performance as this may
    // be called multiple times for each mouse drag.
    static bool has_crash_reported = false;
    if (!has_crash_reported) {
      static crash_reporter::CrashKeyString<20> key("active_tab");
      key.Set(base::NumberToString(active_index));
      base::debug::DumpWithoutCrashing();
      has_crash_reported = true;
    }
    return false;
  }
#endif  // BUILDFLAG(IS_MAC)

  if (model->IsTabBlocked(active_index)) {
    return false;
  }
#endif  // BUILDFLAG(USE_AURA)

  // We don't allow drops on windows that don't have tabstrips.
  if (!other_browser->SupportsWindowFeature(
          Browser::WindowFeature::kFeatureTabStrip)) {
    return false;
  }

  Browser* browser = BrowserView::GetBrowserViewForNativeWindow(
                         GetAttachedBrowserWidget()->GetNativeWindow())
                         ->browser();

  // Profiles must be the same.
  if (other_browser->profile() != browser->profile()) {
    return false;
  }

  // Ensure that browser types and app names are the same.
  if (other_browser->type() != browser->type() ||
      (browser->is_type_app() &&
       browser->app_name() != other_browser->app_name())) {
    return false;
  }

  return true;
}

int TabDragController::GetOutOfBoundsYCoordinate() const {
  DCHECK(attached_context_);
  return attached_context_->GetBoundsInScreen().y() - kVerticalDetachMagnetism -
         1;
}

void TabDragController::NotifyEventIfTabAddedToGroup() {
  if (!source_context_ || source_context_->GetTabStripModel() == nullptr) {
    return;
  }

  const TabStripModel* source_model = source_context_->GetTabStripModel();
  for (const TabDragData& tab_drag_datum : drag_data_.tab_drag_data_) {
    // If the tab already had a group, skip it.
    if (tab_drag_datum.tab_group_data.has_value()) {
      continue;
    }

    // Get the tab group from the source model.
    std::optional<tab_groups::TabGroupId> group_id =
        source_model->GetTabGroupForTab(
            source_model->GetIndexOfWebContents(tab_drag_datum.contents));

    // If there was a tab group for that tab, then send the custom event for
    // adding a tab to a group.
    if (!group_id.has_value()) {
      continue;
    }

    if (views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
            kTabGroupedCustomEventId, tab_drag_datum.attached_view)) {
      break;
    }
  }
}

void TabDragController::MaybePauseTrackingSavedTabGroup() {
  if (!drag_data_.group_drag_data_.has_value()) {
    return;
  }

  const Browser* const browser =
      BrowserView::GetBrowserViewForNativeWindow(
          GetAttachedBrowserWidget()->GetNativeWindow())
          ->browser();

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());

  if (!tab_group_service ||
      !tab_group_service->GetGroup(drag_data_.group_drag_data_.value().group)) {
    return;
  }

  observation_pauser_ = tab_group_service->CreateScopedLocalObserverPauser();
}

void TabDragController::MaybeResumeTrackingSavedTabGroup() {
  if (!drag_data_.group_drag_data_.has_value() || !observation_pauser_) {
    return;
  }

  const Browser* const browser =
      BrowserView::GetBrowserViewForNativeWindow(
          GetAttachedBrowserWidget()->GetNativeWindow())
          ->browser();

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());

  if (!tab_group_service) {
    return;
  }

  observation_pauser_.reset();
}

void TabDragController::UpdateSelectionModel(
    TabStripModel* tab_strip_model,
    ui::ListSelectionModel selection_model) {
  if (selection_model.active().has_value()) {
    std::optional<split_tabs::SplitTabId> split_id =
        tab_strip_model->GetSplitForTab(selection_model.active().value());
    if (split_id.has_value()) {
      selection_model.set_active(split_tabs::GetIndexOfLastActiveTab(
          tab_strip_model, split_id.value()));
    }
  }

  if (selection_model.anchor().has_value()) {
    std::optional<split_tabs::SplitTabId> split_id =
        tab_strip_model->GetSplitForTab(selection_model.anchor().value());
    if (split_id.has_value()) {
      selection_model.set_anchor(split_tabs::GetIndexOfLastActiveTab(
          tab_strip_model, split_id.value()));
    }
  }

  tab_strip_model->SetSelectionFromModel(selection_model);
}

void TabDragController::StartDraggingTabsSession(
    bool initial_move,
    gfx::Point start_point_in_screen) {
  CHECK(current_state_ == DragState::kDraggingTabs ||
        current_state_ == DragState::kWaitingToExitRunLoop);
  CHECK_EQ(dragging_tabs_session_, nullptr);

  dragging_tabs_session_ = std::make_unique<DraggingTabsSession>(
      drag_data_, attached_context_, offset_to_width_ratio_, initial_move,
      start_point_in_screen);
}

#if defined(USE_AURA)
void TabDragController::OnDragStarted() {
  VLOG(1) << __func__;
  if (drag_started_callback_) {
    std::move(drag_started_callback_).Run();
  }
}

void TabDragController::OnDragDropClientDestroying() {
  drag_drop_client_observation_.Reset();
}
#endif  // defined(USE_AURA)
