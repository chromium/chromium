// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_auto_reset.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "chrome/browser/ui/views/tabs/tab_strip_scroll_session.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/tab_groups/tab_group_id.h"
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
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/drag_utils.h"
#include "ui/views/event_monitor.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/window_properties.h"  // nogncheck
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

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
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

// Some platforms, such as Lacros and Desktop Linux with Wayland, disallow
// client applications to manipulate absolute screen positions, by design.
// Preventing, for example, clients from programmatically positioning toplevel
// windows using absolute coordinates. By default, this class assumes that the
// underlying platform supports it, unless indicated by the Ozone platform
// properties.
bool PlatformProvidesAbsoluteWindowPositions() {
#if BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .supports_global_screen_coordinates;
#else
  return true;
#endif
}

constexpr char kTabDraggingPresentationTimeHistogram[] =
    "Browser.TabDragging.PresentationTime";
constexpr char kTabDraggingPresentationTimeMaxHistogram[] =
    "Browser.TabDragging.PresentationTimeMax";
constexpr char kDragAmongTabsPresentationTimeHistogram[] =
    "Browser.TabDragging.DragAmongTabsPresentationTime";
constexpr char kDragToNewBrowserPresentationTimeHistogram[] =
    "Browser.TabDragging.DragToNewBrowserPresentationTime";

#if BUILDFLAG(IS_CHROMEOS)

// Returns the aura::Window which stores the window properties for tab-dragging.
aura::Window* GetWindowForTabDraggingProperties(const TabDragContext* context) {
  return context ? context->GetWidget()->GetNativeWindow() : nullptr;
}

// Returns true if |context| browser window is snapped.
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

// TODO(crbug.com/41482188): This should take a weak ref and return a Liveness,
// because setting capture may cause the drag to end and the drag controller to
// be destroyed.
void SetCapture(TabDragContext* context) {
  context->GetWidget()->SetCapture(context);
}

gfx::Rect GetTabstripScreenBounds(const TabDragContext* context) {
  const views::View* view = context;
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

  for (auto& rect : *rects)
    rect.set_x(rect.x() + x_offset);
}

bool IsWindowDragUsingSystemDragDropAllowed() {
  return base::FeatureList::IsEnabled(
      features::kAllowWindowDragUsingSystemDragDrop);
}

void UpdateSystemDnDDragImage(TabDragContext* attached_context,
                              const gfx::ImageSkia& image) {
#if BUILDFLAG(IS_LINUX)
  aura::Window* root_window =
      attached_context->GetWidget()->GetNativeWindow()->GetRootWindow();
  if (aura::client::GetDragDropClient(root_window)) {
    aura::client::GetDragDropClient(root_window)
        ->UpdateDragImage(image, {image.height() / 2, image.width() / 2});
  }
#endif  // BUILDFLAG(IS_LINUX)
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
      if (TabDragController::IsSystemDragAndDropSessionRunning()) {
        TabDragController::OnSystemDragAndDropEnded();
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
  const raw_ptr<TabDragController, DanglingUntriaged> parent_;
};

TabDragController::TabDragData::TabDragData()
    : contents(nullptr),
      source_model_index(std::nullopt),
      attached_view(nullptr),
      pinned(false) {}

TabDragController::TabDragData::~TabDragData() = default;

TabDragController::TabDragData::TabDragData(TabDragData&&) = default;

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
      source_view_index_(std::numeric_limits<size_t>::max()),
      initial_move_(true),
      detach_behavior_(DETACHABLE),
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
  if (g_tab_drag_controller == this)
    g_tab_drag_controller = nullptr;

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
    const std::vector<raw_ptr<TabSlotView, VectorExperimental>>& dragging_views,
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
  ref->can_release_capture_ = false;
#endif
  ref->start_point_in_screen_ =
      gfx::Point(source_view_offset, mouse_offset.y());
  views::View::ConvertPointToScreen(source_view,
                                    &(ref->start_point_in_screen_));
  ref->event_source_ = event_source;
  ref->mouse_offset_ = mouse_offset;
  ref->last_point_in_screen_ = start_point_in_screen_;
  // Detachable tabs are not supported on Mac if the window is an out-of-process
  // (remote_cocoa) window, i.e. a PWA window.
  // TODO(crbug.com/40128833): Make detachable tabs work in PWAs on Mac.
#if BUILDFLAG(IS_MAC)
  if (ref->source_context_->GetWidget() &&
      remote_cocoa::IsWindowRemote(
          ref->source_context_->GetWidget()->GetNativeWindow())) {
    ref->detach_behavior_ = NOT_DETACHABLE;
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (source_browser->IsLockedForOnTask()) {
    ref->detach_behavior_ = NOT_DETACHABLE;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (source_browser->app_controller() &&
      source_browser->app_controller()->has_tab_strip() &&
      web_app::HasPinnedHomeTab(source_browser->tab_strip_model())) {
    for (TabSlotView* dragging_view : dragging_views) {
      if (source_context->GetIndexOf(dragging_view) == 0) {
        ref->detach_behavior_ = NOT_DETACHABLE;
      }
    }
  }
#endif  // BUILDFLAG(IS_MAC)

  const gfx::Point start_point_in_source_context =
      views::View::ConvertPointFromScreen(source_context,
                                          ref->start_point_in_screen_);
  ref->last_move_attached_context_loc_ = start_point_in_source_context.x();

  ref->source_context_emptiness_tracker_ =
      std::make_unique<SourceTabStripEmptinessTracker>(
          ref->source_context_->GetTabStripModel(), this);

  ref->header_drag_ = source_view->GetTabSlotViewType() ==
                      TabSlotView::ViewType::kTabGroupHeader;
  if (ref->header_drag_) {
    ref->group_ = source_view->group();
  }

  ref->drag_data_.resize(dragging_views.size());
  for (size_t i = 0; i < dragging_views.size(); ++i) {
    ref->InitDragData(dragging_views[i], &(ref->drag_data_[i]));
  }
  ref->source_view_index_ =
      base::ranges::find(dragging_views, source_view) - dragging_views.begin();

  // Listen for Esc key presses and mouse releases.
  ref->event_tracker_ = std::make_unique<EventTracker>(
      base::BindOnce(&TabDragController::EndDrag, base::Unretained(this),
                     END_DRAG_COMPLETE),
      base::BindOnce(&TabDragController::EndDrag, base::Unretained(this),
                     END_DRAG_CANCEL),
      ref->source_context_->GetWidget()->GetNativeWindow());

  if (source_view->width() > 0) {
    ref->offset_to_width_ratio_ =
        static_cast<float>(
            source_view->GetMirroredXInView(source_view_offset)) /
        source_view->width();
  }
  ref->InitWindowCreatePoint();
  ref->initial_selection_model_ = std::move(initial_selection_model);

  ref->window_finder_ = std::make_unique<WindowFinder>();

  if (base::FeatureList::IsEnabled(tabs::kScrollableTabStrip) &&
      base::FeatureList::IsEnabled(tabs::kScrollableTabStripWithDragging)) {
    const int drag_with_scroll_mode = base::GetFieldTrialParamByFeatureAsInt(
        tabs::kScrollableTabStripWithDragging,
        tabs::kTabScrollingWithDraggingModeName, 1);

    switch (drag_with_scroll_mode) {
      case static_cast<int>(ScrollWithDragStrategy::kConstantSpeed):
        ref->drag_with_scroll_mode_ = ScrollWithDragStrategy::kConstantSpeed;
        ref->tab_strip_scroll_session_ =
            std::make_unique<TabStripScrollSessionWithTimer>(
                *this, TabStripScrollSessionWithTimer::ScrollSessionTimerType::
                           kConstantTimer);
        break;
      case static_cast<int>(ScrollWithDragStrategy::kVariableSpeed):
        ref->drag_with_scroll_mode_ = ScrollWithDragStrategy::kVariableSpeed;
        ref->tab_strip_scroll_session_ =
            std::make_unique<TabStripScrollSessionWithTimer>(
                *this, TabStripScrollSessionWithTimer::ScrollSessionTimerType::
                           kVariableTimer);
        break;
      default:
        NOTREACHED();
    }
  }

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
    SetCapture(ref->source_context_);
    if (!ref) {
      return Liveness::DELETED;
    }
  }

  CHECK(ref);
  return Liveness::ALIVE;
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
bool TabDragController::IsSystemDragAndDropSessionRunning() {
  return g_tab_drag_controller &&
         g_tab_drag_controller->system_drag_and_drop_session_running_;
}

// static
void TabDragController::OnSystemDragAndDropUpdated(
    const ui::DropTargetEvent& event) {
  CHECK(IsSystemDragAndDropSessionRunning());
  VLOG(2) << __func__ << " event=" << event.ToString();
  // It is important to use the event's root location instead of its location.
  // The latter may have been transformed to be relative to a child window, e.g.
  // Aura's clipping window. But we need the location relative to the browser
  // window, i.e. the root location.
  g_tab_drag_controller->Drag(event.root_location());
}

// static
void TabDragController::OnSystemDragAndDropExited() {
  CHECK(IsSystemDragAndDropSessionRunning());
  VLOG(1) << __func__;
  // Call Drag() with a location that is definitely out of the tab strip.
  g_tab_drag_controller->Drag(
      {g_tab_drag_controller->last_point_in_screen_.x(),
       g_tab_drag_controller->GetOutOfBoundsYCoordinate()});
}

// static
void TabDragController::OnSystemDragAndDropEnded() {
  CHECK(IsSystemDragAndDropSessionRunning());
  VLOG(1) << __func__;
  // Set this to prevent us from cancelling the drag again. The platform might
  // not have finished processing the drag end, and requesting to cancel the
  // drag might put us in an infinite loop of being notified about the drag end,
  // requesting to cancel the drag, being notified again, and so on.
  g_tab_drag_controller->system_drag_and_drop_session_running_ = false;
  g_tab_drag_controller->EndDrag(END_DRAG_COMPLETE);
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
  // Note: When we're in the kDraggingUsingSystemDragAndDrop state, this method
  // being called means a tab was added to the source window of the drag. We
  // don't need to cancel the drag in that case.
  if (current_state_ == DragState::kDraggingWindow && !is_mutating_) {
    EndDrag(END_DRAG_COMPLETE);
  }
}

void TabDragController::OnTabWillBeRemoved(content::WebContents* contents) {
  // End the drag before we remove a tab that's being dragged, to avoid
  // complex special cases that could result.
  if (!CanRemoveTabDuringDrag(contents))
    EndDrag(END_DRAG_COMPLETE);
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

void TabDragController::Drag(const gfx::Point& point_in_screen) {
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
    return;
  }

  bring_to_front_timer_.Stop();

  if (current_state_ == DragState::kNotStarted) {
    if (!CanStartDrag(point_in_screen))
      return;  // User hasn't dragged far enough yet.

    // If any of the tabs have disappeared (e.g. closed or discarded), cancel
    // the drag session. See crbug.com/1445776.
    if (GetViewsMatchingDraggedContents(source_context_).empty()) {
      EndDrag(END_DRAG_CANCEL);
      return;
    }

    // On windows SaveFocus() may trigger a capture lost, which destroys us.
    {
      base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
      SaveFocus();
      if (!ref)
        return;
    }
    current_state_ = DragState::kDraggingTabs;
    // |source_context_| already owns |this| (it created us, even), so no need
    // to hand off ownership.
    DCHECK_EQ(source_context_->GetDragController(), this);
    Attach(source_context_, gfx::Point(), nullptr);
    if (num_dragging_tabs() == source_context_->GetTabStripModel()->count()) {
      if (ShouldDragWindowUsingSystemDnD()) {
        StartSystemDnDSessionIfNecessary(attached_context_, point_in_screen);
        return;
      }

      views::Widget* widget = GetAttachedBrowserWidget();
      gfx::Rect new_bounds;
      gfx::Vector2d drag_offset;
      if (was_source_maximized_ ||
          (was_source_fullscreen_ && CanRestoreFullscreenWindowDuringDrag())) {
        did_restore_window_ = true;
        // When all tabs in a maximized browser are dragged the browser gets
        // restored during the drag and maximized back when the drag ends.
        const int previous_tab_area_width =
            attached_context_->GetTabDragAreaWidth();
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
        AdjustBrowserAndTabBoundsForDrag(previous_tab_area_width,
                                         point_in_screen, &drag_offset,
                                         &drag_bounds);
        widget->SetVisibilityChangedAnimationsEnabled(true);
      } else {
        new_bounds =
            CalculateNonMaximizedDraggedBrowserBounds(widget, point_in_screen);
        widget->SetBounds(new_bounds);
        drag_offset = GetWindowOffset(point_in_screen);
      }

      RunMoveLoop(drag_offset);
      return;
    }
  }

  if (ContinueDragging(point_in_screen) == Liveness::DELETED)
    return;
}

void TabDragController::EndDrag(EndDragReason reason) {
  TRACE_EVENT0("views", "TabDragController::EndDrag");
  presentation_time_recorder_.reset();

  if (!active()) {
    return;
  }

  if (tab_strip_scroll_session_)
    tab_strip_scroll_session_->Stop();

  // Some drags need to react to the model being mutated before the model can
  // change its state.
  if (reason == END_DRAG_MODEL_ADDED_TAB) {
    // if the drag is not a header drag, ignore this signal. We must place the
    // drag at the current position in the tabstrip or else we will be
    // re-entering into tabstrip mutation code.
    if (header_drag_)
      EndDragImpl(source_context_ == attached_context_ ? CANCELED : NORMAL);
    return;
  }

  // If we're dragging a window ignore capture lost since it'll ultimately
  // trigger the move loop to end and we'll revert the drag when RunMoveLoop()
  // finishes.
  if (reason == END_DRAG_CAPTURE_LOST &&
      current_state_ == DragState::kDraggingWindow) {
    return;
  }

  // We always lose capture when hiding |attached_context_|, just ignore it.
  if (reason == END_DRAG_CAPTURE_LOST &&
      current_state_ == DragState::kDraggingUsingSystemDragAndDrop) {
    return;
  }

  // End the move loop if we're in one. Note that the drag will end (just below)
  // before the move loop actually exits.
  if (current_state_ == DragState::kDraggingWindow && in_move_loop_) {
    VLOG(1) << "EndMoveLoop in EndDrag";
    GetAttachedBrowserWidget()->EndMoveLoop();
  }

  EndDragImpl(reason != END_DRAG_COMPLETE && source_context_ ? CANCELED
                                                             : NORMAL);
}

void TabDragController::SetDragLoopDoneCallbackForTesting(
    base::OnceClosure callback) {
  drag_loop_done_callback_ = std::move(callback);
}

void TabDragController::InitDragData(TabSlotView* view,
                                     TabDragData* drag_data) {
  TRACE_EVENT0("views", "TabDragController::InitDragData");
  const std::optional<int> source_model_index =
      source_context_->GetIndexOf(view);
  drag_data->source_model_index = source_model_index;
  if (source_model_index.has_value()) {
    drag_data->contents = source_context_->GetTabStripModel()->GetWebContentsAt(
        drag_data->source_model_index.value());
    drag_data->pinned = source_context_->IsTabPinned(static_cast<Tab*>(view));
  }
  std::optional<tab_groups::TabGroupId> tab_group_id = view->group();
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
      source_context_->GetWidget()->GetFocusManager()->GetFocusedView());
  source_context_->GetWidget()->GetFocusManager()->ClearFocus();
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

  DCHECK(attached_context_);

  TabDragContext* target_context = source_context_;
  if (detach_behavior_ == DETACHABLE &&
      GetTargetTabStripForPoint(point_in_screen, &target_context) ==
          Liveness::DELETED) {
    return Liveness::DELETED;
  }

  const bool tab_strip_changed = (target_context != attached_context_);
  last_point_in_screen_ = point_in_screen;

  if (tab_strip_changed) {
    is_dragging_new_browser_ = false;
    did_restore_window_ = false;

    if (DragBrowserToNewTabStrip(target_context, point_in_screen) ==
        DRAG_BROWSER_RESULT_STOP) {
      // TODO(crbug.com/41482188): This may not always be correct.
      // `DragBrowserToNewTabStrip` can delete `this` in some cases.
      return Liveness::ALIVE;
    }
  }

  if (current_state_ == DragState::kDraggingWindow) {
    bring_to_front_timer_.Start(
        FROM_HERE, base::Milliseconds(750),
        base::BindOnce(&TabDragController::BringWindowUnderPointToFront,
                       base::Unretained(this), point_in_screen));
  }

  if (current_state_ == DragState::kDraggingTabs) {
    MoveAttached(point_in_screen, false);
    if (tab_strip_changed) {
      // Move the corresponding window to the front. We do this after the
      // move as on windows activate triggers a synchronous paint.
      attached_context_->GetWidget()->Activate();
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
      attached_native_view, target_context->GetWidget()->GetNativeView(),
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
    // Disable animations so that we don't see a close animation on aero.
    browser_widget->SetVisibilityChangedAnimationsEnabled(false);
    if (can_release_capture_)
      browser_widget->ReleaseCapture();
    else
      SetCapture(target_context);

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
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
      // We already transferred ownership of |this| above, before we released
      // capture.
      DetachAndAttachToNewContext(DONT_RELEASE_CAPTURE, target_context,
                                  point_in_screen);

      // Enter kWaitingToExitRunLoop until we actually have exited the nested
      // run loop. Otherwise, we might attempt to start another nested run loop,
      // which will CHECK. See https://crbug.com/41493121.
      current_state_ = DragState::kWaitingToExitRunLoop;

      // Move the tabs into position.
      MoveAttached(point_in_screen, true);
      attached_context_->GetWidget()->Activate();
    }

    return DRAG_BROWSER_RESULT_STOP;
  }

  if (current_state_ == DragState::kDraggingUsingSystemDragAndDrop) {
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
  DetachAndAttachToNewContext(DONT_RELEASE_CAPTURE, target_context,
                              point_in_screen);
  MoveAttached(point_in_screen, true);
  return DRAG_BROWSER_RESULT_CONTINUE;
}

bool TabDragController::ShouldDragWindowUsingSystemDnD() {
  return IsWindowDragUsingSystemDragDropAllowed() &&
         !GetAttachedBrowserWidget()->IsMoveLoopSupported();
}

gfx::ImageSkia TabDragController::GetDragImageForSystemDnD() {
  // The width has the same value, as the logo image is square-shaped.
  const int drag_image_height = 50;
  const gfx::Size drag_image_size(drag_image_height, drag_image_height);
  gfx::ImageSkia drag_image =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PRODUCT_LOGO_256);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      drag_image, skia::ImageOperations::RESIZE_BEST, drag_image_size);
}

TabDragController::Liveness TabDragController::StartSystemDnDSessionIfNecessary(
    TabDragContext* context,
    const gfx::Point& point_in_screen) {
  CHECK(IsWindowDragUsingSystemDragDropAllowed());
  CHECK(ui::ResourceBundle::HasSharedInstance());
  current_state_ = DragState::kDraggingUsingSystemDragAndDrop;

  if (system_drag_and_drop_session_running_) {
    // Show the drag image again.
    gfx::ImageSkia drag_image = GetDragImageForSystemDnD();
    UpdateSystemDnDDragImage(attached_context_, drag_image);

    return Liveness::ALIVE;
  }

  system_drag_and_drop_session_running_ = true;

  if (attached_context_ == source_context_ &&
      num_dragging_tabs() == source_context_->GetTabStripModel()->count()) {
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
      ui::ClipboardFormatType::GetType(ui::kMimeTypeWindowDrag), pickle);

  gfx::ImageSkia drag_image = GetDragImageForSystemDnD();
  data_provider->SetDragImage(
      drag_image,
      gfx::Vector2d(drag_image.height() / 2, drag_image.width() / 2));

  // Pull into a local to avoid use-after-free if RunShellDrag deletes |this|.
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
  if (ref && current_state_ == DragState::kDraggingUsingSystemDragAndDrop) {
    VLOG(1) << __func__ << " Ending drag";
    EndDrag(END_DRAG_COMPLETE);
  }

  if (drag_loop_done_callback) {
    std::move(drag_loop_done_callback).Run();
  }

  return ref ? Liveness::ALIVE : Liveness::DELETED;
}

void TabDragController::HideAttachedContext() {
  CHECK_EQ(current_state_, DragState::kDraggingUsingSystemDragAndDrop);
  CHECK(GetAttachedBrowserWidget()->IsVisible());

  // See the comment in StartSystemDnDSessionIfNecessary() where we start the
  // drag session.
  GetAttachedBrowserWidget()->Hide();

  // Stop observing, as we're only interested in knowing when the drag started.
#if defined(USE_AURA)
  drag_drop_client_observation_.Reset();
#endif  // defined(USE_AURA)
}

gfx::Rect TabDragController::GetEnclosingRectForDraggedTabs() {
  CHECK_GT(drag_data_.size(), 0UL);

  const TabSlotView* const last_tab = drag_data_.back().attached_view;
  const TabSlotView* const first_tab = drag_data_.front().attached_view;

  DCHECK(attached_context_);
  DCHECK(first_tab->parent() == attached_context_);

  const gfx::Point right_point_of_last_tab = last_tab->bounds().bottom_right();
  const gfx::Point left_point_of_first_tab = first_tab->bounds().origin();

  return gfx::Rect(left_point_of_first_tab.x(), 0,
                   right_point_of_last_tab.x() - left_point_of_first_tab.x(),
                   0);
}

gfx::Point TabDragController::GetLastPointInScreen() {
  return last_point_in_screen_;
}

bool TabDragController::IsDraggingTabState() {
  return current_state_ == DragState::kDraggingTabs;
}

views::View* TabDragController::GetAttachedContext() {
  return attached_context_;
}

views::ScrollView* TabDragController::GetScrollView() {
  return attached_context_->GetScrollView();
}

void TabDragController::MoveAttached(const gfx::Point& point_in_screen,
                                     bool just_attached) {
  DCHECK(attached_context_);
  DCHECK(current_state_ == DragState::kDraggingTabs ||
         current_state_ == DragState::kWaitingToExitRunLoop)
      << "MoveAttached called with invalid DragState "
      << static_cast<std::underlying_type<DragState>::type>(current_state_);

  gfx::Point dragged_view_point = GetAttachedDragPoint(point_in_screen);

  const int threshold = attached_context_->GetHorizontalDragThreshold();

  std::vector<raw_ptr<TabSlotView, VectorExperimental>> views(
      drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    views[i] = drag_data_[i].attached_view.get();

  bool did_layout = false;

  gfx::Point point_in_attached_context =
      gfx::Point(point_in_screen.x(), point_in_screen.y());
  views::View::ConvertPointFromScreen(attached_context_,
                                      &point_in_attached_context);

  // Update the model, moving the WebContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter), or if this the first move and the tabs are not consecutive, or if
  // we have just attached to a new tabstrip and need to move to the correct
  // initial position.
  if (just_attached ||
      (abs(point_in_attached_context.x() - last_move_attached_context_loc_) >
       threshold) ||
      (initial_move_ && !AreTabsConsecutive())) {
    TabStripModel* attached_model = attached_context_->GetTabStripModel();
    int to_index = attached_context_->GetInsertionIndexForDraggedBounds(
        GetDraggedViewTabStripBounds(dragged_view_point),
        GetViewsMatchingDraggedContents(attached_context_), num_dragging_tabs(),
        group_);

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

    // Only record the metric when the tab is moved to a different index.
    if (!just_attached && index_of_last_item != to_index) {
      attached_context_->GetWidget()
          ->GetCompositor()
          ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
              [](base::TimeTicks now,
                 const viz::FrameTimingDetails& frame_timing_details) {
                base::TimeTicks presentation_timestamp =
                    frame_timing_details.presentation_feedback.timestamp;
                UmaHistogramTimes(kDragAmongTabsPresentationTimeHistogram,
                                  presentation_timestamp - now);
              },
              base::TimeTicks::Now()));
    }

    if (header_drag_) {
      attached_model->MoveGroupTo(group_.value(), to_index);
    } else {
      attached_model->MoveSelectedTabsTo(
          to_index, CalculateGroupForDraggedTabs(to_index));
    }

    // Move may do nothing in certain situations (such as when dragging pinned
    // tabs). Make sure the tabstrip actually changed before updating
    // `last_move_attached_context_loc_`.
    if (index_of_last_item !=
        attached_model->GetIndexOfWebContents(last_contents)) {
      last_move_attached_context_loc_ = point_in_attached_context.x();
    }
  }

  // Let stop be handled by the callback of `tab_strip_scroll_session_`
  if (tab_strip_scroll_session_)
    tab_strip_scroll_session_->MaybeStart();

  if (!did_layout) {
    attached_context_->LayoutDraggedViewsAt(
        views, source_view_drag_data()->attached_view, dragged_view_point,
        initial_move_);
  }

  initial_move_ = false;

  // Snap the non-dragged tabs to their ideal bounds now, otherwise those tabs
  // will animate to those bounds after attach, which looks flickery/bad. See
  // https://crbug.com/1360330.
  if (just_attached)
    attached_context_->ForceLayout();
}

TabDragController::DetachPosition TabDragController::GetDetachPosition(
    const gfx::Point& point_in_screen) {
  DCHECK(attached_context_);
  gfx::Point attached_point(point_in_screen);
  views::View::ConvertPointFromScreen(attached_context_, &attached_point);
  if (attached_point.x() < attached_context_->TabDragAreaBeginX())
    return DETACH_BEFORE;
  if (attached_point.x() >= attached_context_->TabDragAreaEndX())
    return DETACH_AFTER;
  return DETACH_ABOVE_OR_BELOW;
}

void TabDragController::DetachAndAttachToNewContext(
    ReleaseCapture release_capture,
    TabDragContext* target_context,
    const gfx::Point& point_in_screen,
    bool set_capture) {
  std::unique_ptr<TabDragController> me = Detach(release_capture);
  DCHECK_EQ(me.get(), this);
  DCHECK(!target_context->GetDragController());
  Attach(target_context, point_in_screen, std::move(me), set_capture);
}

TabDragController::Liveness TabDragController::GetTargetTabStripForPoint(
    const gfx::Point& point_in_screen,
    TabDragContext** context) {
  *context = nullptr;
  TRACE_EVENT1("views", "TabDragController::GetTargetTabStripForPoint",
               "point_in_screen", point_in_screen.ToString());

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
    if (destination_tab_strip &&
        DoesTabStripContain(destination_tab_strip, point_in_screen)) {
      *context = destination_tab_strip;
      return Liveness::ALIVE;
    }
  }

  *context = current_state_ == DragState::kDraggingWindow
                 ? attached_context_.get()
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
                               std::unique_ptr<TabDragController> controller,
                               bool set_capture) {
  TRACE_EVENT1("views", "TabDragController::Attach", "point_in_screen",
               point_in_screen.ToString());

  DCHECK(!attached_context_);  // We should already have detached by the time
                               // we get here.

  if (controller) {
    // |this| may be owned by |controller|.
    DCHECK_EQ(this, controller.get());
    DCHECK(!attached_context->GetDragController());
  } else {
    // Or |this| may be owned by |attached_context|
    DCHECK_EQ(this, attached_context->GetDragController());
  }

  attached_context_ = attached_context;

  std::vector<raw_ptr<TabSlotView, VectorExperimental>> views =
      GetViewsMatchingDraggedContents(attached_context_);

  if (views.empty()) {
    // Transitioning from detached to attached to a new context. Add tabs to
    // the new model.

    selection_model_before_attach_ =
        attached_context->GetTabStripModel()->selection_model();

    // Register a new group if necessary, so that the insertion index in the
    // tab strip can be calculated based on the group membership of tabs.
    if (header_drag_) {
      const tab_groups::TabGroupVisualData og_visual_data =
          source_view_drag_data()->tab_group_data.value().group_visual_data;
      // Create the new group already un-collapsed, regardless of whether the
      // original group started out collapsed or not.
      const tab_groups::TabGroupVisualData new_visual_data =
          tab_groups::TabGroupVisualData(og_visual_data.title(),
                                         og_visual_data.color(),
                                         /*is_collapsed=*/false);

      attached_context_->GetTabStripModel()->group_model()->AddTabGroup(
          group_.value(), new_visual_data);
    }

    // Insert at any valid index in the tabstrip. We'll fix up the insertion
    // index in MoveAttached() later.
    int index = attached_context_->GetPinnedTabCount();

    base::AutoReset<bool> setter(&is_mutating_, true);
    for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
      int add_types = AddTabTypes::ADD_NONE;
      if (drag_data_[i].pinned)
        add_types |= AddTabTypes::ADD_PINNED;

      // We should have owned_tab here, this CHECK is used to gather data for
      // https://crbug.com/677806.
      CHECK(drag_data_[i].owned_tab);
      attached_context_->GetTabStripModel()->InsertDetachedTabAt(
          index + i - first_tab_index(), std::move(drag_data_[i].owned_tab),
          add_types, group_);

      // If a sad tab is showing, the SadTabView needs to be updated.
      SadTabHelper* sad_tab_helper =
          SadTabHelper::FromWebContents(drag_data_[i].contents);
      if (sad_tab_helper)
        sad_tab_helper->ReinstallInWebView();
    }

    views = GetViewsMatchingDraggedContents(attached_context_);

    // If we're dragging a saved group, resume tracking now that the group is
    // re-attached.
    MaybeResumeTrackingSavedTabGroup();
  }
  DCHECK_EQ(views.size(), drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    drag_data_[i].attached_view = views[i].get();
    attached_views_.push_back(views[i]);
  }

  ResetSelection(attached_context_->GetTabStripModel());

  // This should be called after ResetSelection() in order to generate
  // bounds correctly. http://crbug.com/836004
  attached_context_->StartedDragging(views);

  // The size of the dragged tab may have changed. Adjust the x offset so that
  // ratio of mouse_offset_ to original width is maintained.
  std::vector<raw_ptr<TabSlotView, VectorExperimental>> tabs_to_source(views);
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
  if (controller)
    attached_context_->OwnDragController(std::move(controller));

  SetTabDraggingInfo();
  attached_context_tabs_closed_tracker_ =
      std::make_unique<DraggedTabsClosedTracker>(
          attached_context_->GetTabStripModel(), this);
}

std::unique_ptr<TabDragController> TabDragController::Detach(
    ReleaseCapture release_capture) {
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

  if (release_capture == RELEASE_CAPTURE)
    attached_context_->GetWidget()->ReleaseCapture();

  TabStripModel* attached_model = attached_context_->GetTabStripModel();

  // If we're dragging a saved tab group, suspend tracking between Detach and
  // Attach. Otherwise, the group will get emptied out as we close all the tabs.
  MaybePauseTrackingSavedTabGroup();

  for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
    int index = attached_model->GetIndexOfWebContents(drag_data_[i].contents);
    DCHECK_NE(TabStripModel::kNoTab, index);
    // Move the tab out of `attached_model`. Marking the view as detached tells
    // the TabStrip to not animate its closure, as it's actually being moved.
    drag_data_[i].attached_view->set_detached();
    drag_data_[i].owned_tab = attached_model->DetachTabAtForInsertion(index);

    // Detaching may end up deleting the tab, drop references to it.
    drag_data_[i].attached_view = nullptr;
  }
  if (header_drag_)
    source_view_drag_data()->attached_view = nullptr;

  // If we've removed the last Tab from the TabDragContext, hide the
  // frame now.
  if (!attached_model->empty()) {
    if (!selection_model_before_attach_.empty() &&
        selection_model_before_attach_.active().has_value() &&
        selection_model_before_attach_.active().value() <
            static_cast<size_t>(attached_model->count())) {
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

  return me;
}

void TabDragController::DetachIntoNewBrowserAndRunMoveLoop(
    const gfx::Point& point_in_screen) {
  if (attached_context_->GetTabStripModel()->count() == num_dragging_tabs()) {
    // All the tabs in a browser are being dragged but all the tabs weren't
    // initially being dragged. For this to happen the user would have to
    // start dragging a set of tabs, the other tabs close, then detach.

    if (ShouldDragWindowUsingSystemDnD()) {
      StartSystemDnDSessionIfNecessary(attached_context_, point_in_screen);
      return;
    }

    RunMoveLoop(GetWindowOffset(point_in_screen));
    return;
  }

  const int previous_tab_area_width = attached_context_->GetTabDragAreaWidth();
  std::vector<gfx::Rect> drag_bounds =
      attached_context_->CalculateBoundsForDraggedViews(attached_views_);
  OffsetX(GetAttachedDragPoint(point_in_screen).x(), &drag_bounds);

  gfx::Vector2d drag_offset;
  Browser* const browser = CreateBrowserForDrag(
      attached_context_, point_in_screen, &drag_offset, &drag_bounds);

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
    // scenario is why we pass kDontCancel above), but on Lacros it apparently
    // sometimes can. See https://crbug.com/1350564.
    CHECK(ref) << "Drag session was ended as part of transferring events to "
                  "the new browser. This should not happen.";
  }
#endif

  dragged_widget->SetCanAppearInExistingFullscreenSpaces(true);
  dragged_widget->SetVisibilityChangedAnimationsEnabled(false);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, Detach should release capture; |can_release_capture_| is
  // false on ChromeOS because it can cancel touches, but for this cases
  // the touches are already transferred, so releasing is fine. Without
  // releasing, the capture remains and further touch events can be sent to a
  // wrong target.
  const ReleaseCapture release_capture = RELEASE_CAPTURE;
#else
  const ReleaseCapture release_capture =
      can_release_capture_ ? RELEASE_CAPTURE : DONT_RELEASE_CAPTURE;
#endif
  DetachAndAttachToNewContext(
      release_capture, dragged_browser_view->tabstrip()->GetDragContext(),
      gfx::Point());

  if (ShouldDragWindowUsingSystemDnD()) {
    // Keep the new window hidden and start a system DnD session.
    //
    // We need to pass `source_context_` here, because the new context we're
    // attached to is hidden and thus can't start the drag session.
    StartSystemDnDSessionIfNecessary(source_context_, point_in_screen);
    return;
  }

  AdjustBrowserAndTabBoundsForDrag(previous_tab_area_width, point_in_screen,
                                   &drag_offset, &drag_bounds);
  browser->window()->Show();
  dragged_widget->SetVisibilityChangedAnimationsEnabled(true);
  // Activate may trigger a focus loss, destroying us.
  {
    base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
    browser->window()->Activate();
    if (!ref) {
      return;
    }
  }
  RunMoveLoop(drag_offset);
}

void TabDragController::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  CHECK(!ShouldDragWindowUsingSystemDnD());

  move_loop_widget_ = GetAttachedBrowserWidget();
  DCHECK(move_loop_widget_);

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

  // Pull into a local to avoid use-after-free if RunMoveLoop deletes |this|.
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
  // Note: |this| can be deleted here!

  if (drag_loop_done_callback)
    std::move(drag_loop_done_callback).Run();

  if (!ref)
    return;

  in_move_loop_ = false;
  widget_observation_.Reset();
  move_loop_widget_ = nullptr;

  if (current_state_ == DragState::kWaitingToExitRunLoop) {
    current_state_ = DragState::kDraggingTabs;
  } else if (current_state_ == DragState::kWaitingToDragTabs) {
    DCHECK(tab_strip_to_attach_to_after_exit_);
    gfx::Point point_in_screen(GetCursorScreenPoint());
    DetachAndAttachToNewContext(DONT_RELEASE_CAPTURE,
                                tab_strip_to_attach_to_after_exit_,
                                point_in_screen);
    current_state_ = DragState::kDraggingTabs;
    // Move the tabs into position.
    MoveAttached(point_in_screen, true);
    attached_context_->GetWidget()->Activate();
    // Activate may trigger a focus loss, destroying us.
    if (!ref)
      return;
    tab_strip_to_attach_to_after_exit_ = nullptr;
  } else if (current_state_ == DragState::kDraggingWindow) {
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
  views::View::ConvertPointFromScreen(attached_context_, &tab_loc);
  const int x =
      attached_context_->GetMirroredXInView(tab_loc.x()) - mouse_offset_.x();

  // If the width needed for the `attached_views_` is greater than what is
  // available in the tab drag area the attached drag point should simply be the
  // beginning of the tab strip. Once attached the `attached_views_` will simply
  // overflow as usual (see https://crbug.com/1250184).
  const int max_x =
      std::max(0, attached_context_->GetTabDragAreaWidth() -
                      TabStrip::GetSizeNeededForViews(attached_views_));
  return gfx::Point(std::clamp(x, 0, max_x), 0);
}

std::vector<raw_ptr<TabSlotView, VectorExperimental>>
TabDragController::GetViewsMatchingDraggedContents(TabDragContext* context) {
  const TabStripModel* const model = context->GetTabStripModel();
  std::vector<raw_ptr<TabSlotView, VectorExperimental>> views;
  for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
    const int model_index =
        model->GetIndexOfWebContents(drag_data_[i].contents);
    if (model_index == TabStripModel::kNoTab)
      return std::vector<raw_ptr<TabSlotView, VectorExperimental>>();
    views.push_back(context->GetTabAt(model_index));
  }
  if (header_drag_)
    views.insert(views.begin(), context->GetTabGroupHeader(group_.value()));
  return views;
}

void TabDragController::EndDragImpl(EndDragType type) {
  VLOG(1) << __func__ << " type=" << type
          << " state=" << static_cast<int>(current_state_);

  DragState previous_state = current_state_;
  current_state_ = DragState::kStopped;
  attached_context_tabs_closed_tracker_.reset();

  bring_to_front_timer_.Stop();

  // Before clearing the tab drag data check to see if there was a tab that was
  // dragged from out of a tab group into a tab group.
  NotifyEventIfTabAddedToGroup();

  if (type != TAB_DESTROYED) {
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
      if (type == CANCELED) {
        RevertDrag();
      } else {
        if (previous_state == DragState::kDraggingUsingSystemDragAndDrop) {
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
      attached_context_ ? attached_context_.get() : source_context_.get();
  owning_context->DestroyDragController();
}

void TabDragController::RevertDrag() {
  // If we're dragging a saved tab group, suspend tracking during the revert.
  // Otherwise, the group will get emptied out as we revert all the tabs.
  MaybePauseTrackingSavedTabGroup();

  if (header_drag_) {
    RevertHeaderDrag(group_.value());
  } else {
    for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
      if (drag_data_[i].contents) {
        // Contents is NULL if a tab was destroyed while the drag was under way.
        RevertDragAt(i);
      }
    }
  }

  MaybeResumeTrackingSavedTabGroup();

  if (attached_context_) {
    if (did_restore_window_)
      MaximizeAttachedWindow();
    if (attached_context_ == source_context_) {
      source_context_->StoppedDragging();
      if (header_drag_)
        source_context_->GetTabStripModel()->MoveTabGroup(group_.value());
    } else {
      attached_context_->DraggedTabsDetached();
    }
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

  if (initial_selection_model_.empty())
    ResetSelection(source_context_->GetTabStripModel());
  else
    source_context_->GetTabStripModel()->SetSelectionFromModel(
        initial_selection_model_);

  if (source_context_)
    source_context_->GetWidget()->Activate();
}

void TabDragController::ResetSelection(TabStripModel* model) {
  DCHECK(model);
  ui::ListSelectionModel selection_model;
  bool has_one_valid_tab = false;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    // |contents| is NULL if a tab was deleted out from under us.
    if (drag_data_[i].contents) {
      int index = model->GetIndexOfWebContents(drag_data_[i].contents);
      DCHECK_GE(index, 0);
      selection_model.AddIndexToSelection(static_cast<size_t>(index));
      if (!has_one_valid_tab || i == source_view_index_) {
        // Reset the active/lead to the first tab. If the source tab is still
        // valid we'll reset these again later on.
        selection_model.set_active(static_cast<size_t>(index));
        selection_model.set_anchor(static_cast<size_t>(index));
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
  for (const TabDragData& data : base::Reversed(drag_data_)) {
    if (data.source_model_index.has_value())
      selection_model.DecrementFrom(data.source_model_index.value());
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
  if (!selection_model.anchor().has_value())
    selection_model.set_anchor(*selection_model.selected_indices().begin());
  if (!selection_model.active().has_value())
    selection_model.set_active(*selection_model.selected_indices().begin());
  source_context_->GetTabStripModel()->SetSelectionFromModel(selection_model);
}

void TabDragController::RevertHeaderDrag(tab_groups::TabGroupId group_id) {
  CHECK_NE(current_state_, DragState::kNotStarted);
  CHECK(source_context_);

  const size_t first_tab_in_group_index = first_tab_index();
  CHECK(drag_data_[first_tab_in_group_index].contents);

  if (attached_context_ && attached_context_ == source_context_) {
    base::AutoReset<bool> setter(&is_mutating_, true);
    attached_context_->GetTabStripModel()->MoveGroupTo(
        group_id,
        drag_data_[first_tab_in_group_index].source_model_index.value());
  } else {
    for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
      if (drag_data_[i].contents) {
        // Contents is NULL if a tab was destroyed while the drag was under way.
        RevertDragAt(i);
      }
    }
  }
}

void TabDragController::RevertDragAt(size_t drag_index) {
  CHECK_NE(current_state_, DragState::kNotStarted);
  CHECK(source_context_);
  // We can't revert if `contents` was destroyed during the drag, or if this is
  // a group header.
  CHECK(drag_data_[drag_index].contents);

  base::AutoReset<bool> setter(&is_mutating_, true);
  TabDragData* data = &(drag_data_[drag_index]);
  // The index we will try to insert the tab at. It may or may not end up at
  // this index, if the source tabstrip has changed since the drag began.
  int target_index = data->source_model_index.value();

  std::optional<TabDragData::TabGroupData> drag_data = data->tab_group_data;

  // Create the group if not present in the group model.
  const std::optional<tab_groups::TabGroupId> existing_group =
      drag_data.has_value() &&
              source_context_->GetTabStripModel()
                  ->group_model()
                  ->ContainsTabGroup(drag_data.value().group_id)
          ? std::make_optional(drag_data.value().group_id)
          : std::nullopt;

  if (attached_context_) {
    int index = attached_context_->GetTabStripModel()->GetIndexOfWebContents(
        data->contents);
    if (attached_context_ != source_context_) {
      std::unique_ptr<base::AutoReset<bool>> removing_last_tab_setter;
      if (attached_context_->GetTabStripModel()->count() == 1) {
        removing_last_tab_setter = std::make_unique<base::AutoReset<bool>>(
            &is_removing_last_tab_for_revert_, true);
      }
      // The Tab was inserted into another TabDragContext. We need to
      // put it back into the original one. Marking the view as detached tells
      // the TabStrip to not animate its closure, as it's actually being moved.
      data->attached_view->set_detached();
      std::unique_ptr<tabs::TabModel> detached_tab =
          attached_context_->GetTabStripModel()->DetachTabAtForInsertion(index);
      // No-longer removing the last tab, so reset state.
      removing_last_tab_setter.reset();
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      source_context_->GetTabStripModel()->InsertDetachedTabAt(
          target_index, std::move(detached_tab),
          (data->pinned ? AddTabTypes::ADD_PINNED : 0), existing_group);
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
          index, target_index, false, existing_group);
    }
  } else {
    // The Tab was detached from the TabDragContext where the drag
    // began, and has not been attached to any other TabDragContext.
    // We need to put it back into the source TabDragContext.
    source_context_->GetTabStripModel()->InsertDetachedTabAt(
        target_index, std::move(data->owned_tab),
        (data->pinned ? AddTabTypes::ADD_PINNED : 0), existing_group);
  }

  if (drag_data.has_value() && !existing_group.has_value()) {
    source_context_->GetTabStripModel()->AddToNewGroup(
        {target_index}, drag_data.value().group_id,
        drag_data.value().group_visual_data);
  }
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
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_MAC)
      // Keeping maximized state breaks snap to Grid on Windows when dragging
      // tabs from maximized windows. TODO:(crbug.com/727051) Explore doing this
      // for other desktop OS's. kMaximizedStateRetainedOnTabDrag in
      // tab_drag_controller_interactive_uitest.cc will need to be initialized
      // to false on each desktop OS that changes this behavior.
      // macOS opts out since this maps maximize to fullscreen, which can
      // violate user expectations and interacts poorly with some window
      // management actions.
      if (was_source_maximized_ || was_source_fullscreen_)
        MaximizeAttachedWindow();
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
  } else {
    // Compel the model to construct a new window for the detached
    // WebContentses.
    views::Widget* widget = source_context_->GetWidget();
    gfx::Rect window_bounds(widget->GetRestoredBounds());
    window_bounds.set_origin(GetWindowCreatePoint(last_point_in_screen_));

    base::AutoReset<bool> setter(&is_mutating_, true);

    std::vector<TabStripModelDelegate::NewStripContents> tabs;
    for (auto& drag_datum : drag_data_) {
      TabStripModelDelegate::NewStripContents item;
      // We should have owned_contents here, this CHECK is used to gather data
      // for https://crbug.com/677806.
      CHECK(drag_datum.owned_tab);
      item.tab = std::move(drag_datum.owned_tab);
      item.add_types =
          drag_datum.pinned ? AddTabTypes::ADD_PINNED : AddTabTypes::ADD_NONE;
      tabs.push_back(std::move(item));
    }

    Browser* new_browser =
        source_context_->GetTabStripModel()->delegate()->CreateNewStripWithTabs(
            std::move(tabs), window_bounds, widget->IsMaximized());
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
      DCHECK_GE(index, 0);
      selection.AddIndexToSelection(static_cast<size_t>(index));
      selection.set_active(static_cast<size_t>(index));
      selection.set_anchor(static_cast<size_t>(index));
      model->SetSelectionFromModel(selection);
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
  gfx::NativeWindow native_window;
  if (GetLocalProcessWindow(point_in_screen, true, &native_window) ==
      Liveness::DELETED) {
    return;
  }

  // Only bring browser windows to front - only windows with a
  // TabDragContext can be tab drag targets.
  if (!CanAttachTo(native_window))
    return;
}

bool TabDragController::IsDraggingTab(content::WebContents* contents) const {
  for (auto& drag_data : drag_data_) {
    if (drag_data.contents == contents)
      return true;
  }
  return false;
}

views::Widget* TabDragController::GetAttachedBrowserWidget() {
  return attached_context_->GetWidget();
}

bool TabDragController::AreTabsConsecutive() {
  for (size_t i = 1; i < drag_data_.size(); ++i) {
    const std::optional<int> previous_source_index =
        drag_data_[i - 1].source_model_index;
    const std::optional<int> source_index = drag_data_[i].source_model_index;
    if (previous_source_index.has_value() && source_index.has_value() &&
        previous_source_index.value() + 1 != source_index.value()) {
      return false;
    }
  }
  return true;
}

gfx::Rect TabDragController::CalculateDraggedBrowserBounds(
    TabDragContext* source,
    const gfx::Point& point_in_screen,
    std::vector<gfx::Rect>* drag_bounds) {
  // Vertically center the dragged tabs around the mouse.
  gfx::Point center(0, drag_bounds->front().CenterPoint().y());
  views::View::ConvertPointToWidget(source, &center);
  // To support immersive fullscreen on macOS get the top level widget, which
  // will be the browser widget. In immersive fullscreen there are other
  // intermediate widgets that host the toolbar and tab strip, getting their
  // restored bounds is not helpful. Getting the top level widget in the
  // non-immersive fullscreen case is a essentially a NOP, so use it
  // unconditionally here for all platforms.
  gfx::Rect new_bounds(
      source->GetWidget()->GetTopLevelWidget()->GetRestoredBounds());

  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestPoint(last_point_in_screen_)
                            .work_area();
  if (new_bounds.size().width() >= work_area.size().width() &&
      new_bounds.size().height() >= work_area.size().height()) {
    new_bounds = work_area;
    new_bounds.Inset(kMaximizedWindowInset);
    // Behave as if the |source| was maximized at the start of a drag since this
    // is consistent with a browser window creation logic in case of windows
    // that are as large as the |work_area|. Note: Some platforms do not support
    // global screen coordinates tracking, eg: Linux/Wayland, in such cases,
    // avoid this heuristic to determine whether the new browser window should
    // be maximized or not when completing the drag session.
    if (PlatformProvidesAbsoluteWindowPositions())
      was_source_maximized_ = true;
  }

  if (source->GetWidget()->IsMaximized()) {
    // If the restore bounds is really small, we don't want to honor it
    // (dragging a really small window looks wrong), instead make sure the new
    // window is at least 50% the size of the old.
    const gfx::Size max_size(
        source->GetWidget()->GetWindowBoundsInScreen().size());
    new_bounds.set_width(std::max(max_size.width() / 2, new_bounds.width()));
    new_bounds.set_height(std::max(max_size.height() / 2, new_bounds.height()));
  }

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
      break;  // Nothing to do for DETACH_ABOVE_OR_BELOW.
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

  // The user has to move the mouse some amount of pixels before the drag
  // starts. Offset the window by this amount so that the relative offset
  // of the initial location is consistent. See https://crbug.com/518740
  bounds.Offset(point_in_screen.x() - start_point_in_screen_.x(),
                point_in_screen.y() - start_point_in_screen_.y());
  return bounds;
}

void TabDragController::AdjustBrowserAndTabBoundsForDrag(
    int previous_tab_area_width,
    const gfx::Point& point_in_screen,
    gfx::Vector2d* drag_offset,
    std::vector<gfx::Rect>* drag_bounds) {
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
        drag_bounds->front().x() / static_cast<float>(previous_tab_area_width);
    *drag_bounds =
        attached_context_->CalculateBoundsForDraggedViews(attached_views_);

    // If the tabs can fit within the new tab area with room to spare, align
    // them within it so the leading tab is in the same position as it was in
    // the previous tab area, proportionally speaking.
    if (drag_bounds->back().right() < current_tab_area_width) {
      // The tabs must stay within the tabstrip.
      const int maximum_tab_x =
          current_tab_area_width -
          (drag_bounds->back().right() - drag_bounds->front().x());
      const int leading_tab_x =
          std::min(static_cast<int>(leading_ratio * current_tab_area_width),
                   maximum_tab_x);
      OffsetX(leading_tab_x, drag_bounds);
    }

    // Reposition the restored window such that the tab that was dragged remains
    // under the mouse cursor.
    gfx::Rect source_tab_bounds = (*drag_bounds)[source_view_index_];

    int cursor_offset_within_tab =
        base::ClampRound(source_tab_bounds.width() * offset_to_width_ratio_);
    gfx::Point cursor_offset_in_widget(
        attached_context_->GetMirroredXInView(source_tab_bounds.x() +
                                              cursor_offset_within_tab),
        0);
    views::View::ConvertPointToWidget(attached_context_,
                                      &cursor_offset_in_widget);
    gfx::Rect bounds = GetAttachedBrowserWidget()->GetWindowBoundsInScreen();
    bounds.set_x(point_in_screen.x() - cursor_offset_in_widget.x());
    GetAttachedBrowserWidget()->SetBounds(bounds);
    *drag_offset = point_in_screen - bounds.origin();
  }
  attached_context_->SetBoundsForDrag(attached_views_, *drag_bounds);
}

std::optional<webapps::AppId> TabDragController::GetControllingAppForDrag(
    Browser* browser) {
  content::WebContents* active_contents = source_dragged_contents();
  if (!base::FeatureList::IsEnabled(
          features::kTearOffWebAppTabOpensWebAppWindow) ||
      drag_data_.size() != 1 || !active_contents) {
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

Browser* TabDragController::CreateBrowserForDrag(
    TabDragContext* source,
    const gfx::Point& point_in_screen,
    gfx::Vector2d* drag_offset,
    std::vector<gfx::Rect>* drag_bounds) {
  // TODO(crbug.com/40238145): `AttachTabsToNewBrowserOnDrop()` does almost the
  // same as this method. Factor out the common parts into a separate method.

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
  gfx::Rect new_bounds(
      CalculateDraggedBrowserBounds(source, point_in_screen, drag_bounds));
  *drag_offset = point_in_screen - new_bounds.origin();

#if BUILDFLAG(IS_OZONE)
  // On Wayland, for example, coordinates are always relative to the window's
  // origin, and the origin should always be (0, 0). Ensure we set the origin to
  // (0, 0) in this case, or operations like finding the window under the cursor
  // might not work as expected.
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_global_screen_coordinates) {
    // `new_bounds` comes from window bounds, but in tests windows sometimes
    // have a non-(0, 0) origin, so play it safe and explicitly set the origin.
    new_bounds.set_origin({0, 0});
  }
#endif

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
    create_params.initial_bounds = new_bounds;
  }
  create_params.user_gesture = true;
  create_params.in_tab_dragging = true;
#if BUILDFLAG(IS_CHROMEOS)
  // Do not copy attached window's restore id as this will cause Full Restore to
  // restore the newly created browser using the original browser's stored data.
  // See crbug.com/1208923 and crbug.com/1333562 for details.
  create_params.restore_id = Browser::kDefaultRestoreId;

  // Open the window in the same display.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
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

#if !BUILDFLAG(IS_CHROMEOS)
  // If the window is created maximized then the bounds we supplied are ignored.
  // We need to reset them again so they are honored. On ChromeOS, this is
  // handled in NativeWidgetAura.
  if (!open_as_web_app) {
    browser->window()->SetBounds(new_bounds);
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
  return display::Screen::GetScreen()->GetCursorScreenPoint();
#endif
}

gfx::Vector2d TabDragController::GetWindowOffset(
    const gfx::Point& point_in_screen) {
  TabDragContext* owning_context =
      attached_context_ ? attached_context_.get() : source_context_.get();
  views::View* toplevel_view = owning_context->GetWidget()->GetContentsView();

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
        attached_context_->GetWidget()->GetNativeWindow();
    if (dragged_window)
      exclude.insert(dragged_window);
  }
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Exclude windows which are pending deletion via Browser::TabStripEmpty().
  // These windows can be returned in the Linux Aura port because the browser
  // window which was used for dragging is not hidden once all of its tabs are
  // attached to another browser window in DragBrowserToNewTabStrip().
  // TODO(pkotwicz): Fix this properly (crbug.com/358482)
  for (Browser* browser : *BrowserList::GetInstance()) {
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
  dragged_window->SetProperty(ash::kIsDraggingTabsKey, true);
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
#endif
}

std::optional<tab_groups::TabGroupId>
TabDragController::CalculateGroupForDraggedTabs(int to_index) {
  TabStripModel* attached_model = attached_context_->GetTabStripModel();

  // Get the proposed tabstrip model assuming the selection has taken place.
  std::pair<std::optional<int>, std::optional<int>> adjacent_indices =
      attached_model->GetAdjacentTabsAfterSelectedMove(
          base::PassKey<TabDragController>(), to_index);

  const ui::ListSelectionModel::SelectedIndices& selected =
      attached_model->selection_model().selected_indices();

  // Pinned tabs cannot be grouped, so we only change the group membership of
  // unpinned tabs.
  std::vector<int> selected_unpinned;
  for (size_t selected_index : selected) {
    if (!attached_model->IsTabPinned(selected_index))
      selected_unpinned.push_back(selected_index);
  }

  if (selected_unpinned.empty()) {
    return std::nullopt;
  }

  std::optional<tab_groups::TabGroupId> left_group =
      adjacent_indices.first.has_value()
          ? attached_model->GetTabGroupForTab(adjacent_indices.first.value())
          : std::nullopt;
  std::optional<tab_groups::TabGroupId> right_group =
      adjacent_indices.second.has_value()
          ? attached_model->GetTabGroupForTab(adjacent_indices.second.value())
          : std::nullopt;
  std::optional<tab_groups::TabGroupId> current_group =
      attached_model->GetTabGroupForTab(selected_unpinned[0]);

  if (left_group == right_group) {
    return left_group;
  }

  // If the tabs on the left and right have different group memberships,
  // including if one is ungrouped or nonexistent, change the group of the
  // dragged tab based on whether it is "leaning" toward the left or the
  // right of the gap. If the tab is centered in the gap, make the tab
  // ungrouped.

  const Tab* left_most_selected_tab =
      attached_context_->GetTabAt(selected_unpinned[0]);

  const int buffer = left_most_selected_tab->width() / 4;

  // The tab's bounds are larger than what visually appears in order to include
  // space for the rounded feet. Adding {tab_left_inset} to the horizontal
  // bounds of the tab results in the x position that would be drawn when there
  // are no feet showing.
  const int tab_left_inset = TabStyle::Get()->GetTabOverlap() / 2;

  const auto tab_bounds_in_drag_context_coords = [this](int model_index) {
    const Tab* const tab = attached_context_->GetTabAt(model_index);
    return ToEnclosingRect(views::View::ConvertRectToTarget(
        tab, attached_context_, gfx::RectF(tab->GetLocalBounds())));
  };

  // Use the left edge for a reliable fallback, e.g. if this is the leftmost
  // tab or there is a group header to the immediate left.
  int left_edge =
      adjacent_indices.first.has_value()
          ? tab_bounds_in_drag_context_coords(adjacent_indices.first.value())
                    .right() -
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

  if (left_group.has_value() &&
      !attached_model->IsGroupCollapsed(left_group.value())) {
    // Take the dragged tabs out of left_group if they are at the rightmost edge
    // of the tabstrip. This happens when the tabstrip is full and the dragged
    // tabs are as far right as they can go without being pulled out into a new
    // window. In this case, since the dragged tabs can't move further right in
    // the tabstrip, it will never go "beyond" the left_group and therefore
    // never leave it unless we add this check. See crbug.com/1134376.
    // TODO(crbug.com/40842551): Update this to work better with Tab Scrolling
    // once dragging near the end of the tabstrip is cleaner.
    if (tab_bounds_in_drag_context_coords(selected_unpinned.back()).right() >=
        attached_context_->TabDragAreaEndX()) {
      return std::nullopt;
    }

    if (left_most_selected_x_position <= left_edge - buffer)
      return left_group;
  }
  if ((left_most_selected_x_position >= left_edge + buffer) &&
      right_group.has_value() &&
      !attached_model->IsGroupCollapsed(right_group.value())) {
    return right_group;
  }
  return std::nullopt;
}

bool TabDragController::CanAttachTo(gfx::NativeWindow window) {
  if (!window) {
    return false;
  }

  if (window == GetAttachedBrowserWidget()->GetNativeWindow()) {
    return true;
  }

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
  if (wm::GetModalTransient(window))
    return false;
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

int TabDragController::GetOutOfBoundsYCoordinate() const {
  DCHECK(attached_context_);
  return attached_context_->GetBoundsInScreen().y() - kVerticalDetachMagnetism -
         1;
}

void TabDragController::NotifyEventIfTabAddedToGroup() {
  if (!source_context_ || source_context_->GetTabStripModel() == nullptr)
    return;

  const TabStripModel* source_model = source_context_->GetTabStripModel();
  for (size_t i = first_tab_index(); i < drag_data_.size(); ++i) {
    // If the tab already had a group, skip it.
    if (drag_data_[i].tab_group_data.has_value())
      continue;

    // Get the tab group from the source model.
    std::optional<tab_groups::TabGroupId> group_id =
        source_model->GetTabGroupForTab(
            source_model->GetIndexOfWebContents(drag_data_[i].contents));

    // If there was a tab group for that tab, then send the custom event for
    // adding a tab to a group.
    if (!group_id.has_value())
      continue;

    ui::TrackedElement* element =
        views::ElementTrackerViews::GetInstance()->GetElementForView(
            drag_data_[i].attached_view);
    if (!element)
      continue;

    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        element, kTabGroupedCustomEventId);
    break;
  }
}

void TabDragController::MaybePauseTrackingSavedTabGroup() {
  if (!header_drag_) {
    return;
  }

  const Browser* const browser =
      BrowserView::GetBrowserViewForNativeWindow(
          GetAttachedBrowserWidget()->GetNativeWindow())
          ->browser();

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser->profile());

  if (!tab_group_service || !tab_group_service->GetGroup(group_.value())) {
    return;
  }

  observation_pauser_ = tab_group_service->CreateScopedLocalObserverPauser();
}

void TabDragController::MaybeResumeTrackingSavedTabGroup() {
  if (!header_drag_ || !observation_pauser_) {
    return;
  }

  const Browser* const browser =
      BrowserView::GetBrowserViewForNativeWindow(
          GetAttachedBrowserWidget()->GetNativeWindow())
          ->browser();

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser->profile());

  if (!tab_group_service) {
    return;
  }

  observation_pauser_.reset();
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
