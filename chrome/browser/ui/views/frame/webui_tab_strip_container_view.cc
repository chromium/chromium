// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/frame/webui_tab_strip_field_trial.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_colors.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_controller_views.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/webui_tab_counter_button.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_metrics.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/drop_data.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_target.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

// Represents a drag or fling that either goes up or down. Defined here so we
// can use it in module local methods.
enum class WebUITabStripDragDirection { kUp, kDown };

// Represents which type of event is causing the WebUI tab strip to open or
// close. Note that currently |kDragRelease| and |kOther| behave the same but
// they're conceptually different and could use different logic in the future.
enum class WebUITabStripOpenCloseReason {
  // User drags the toolbar up or down and releases it partway.
  kDragRelease,
  // User flings, flicks, or swipes the toolbar up or down (possibly during a
  // drag).
  kFling,
  // The tabstrip is opened or closed as the result of some other action or
  // event not tied to the user directly manipulating the toolbar.
  kOther
};

namespace {

// Returns the animation curve to use for different types of events that could
// cause the tabstrip to be revealed or hidden.
gfx::Tween::Type GetTweenTypeForTabstripOpenClose(
    WebUITabStripOpenCloseReason reason) {
  switch (reason) {
    case WebUITabStripOpenCloseReason::kDragRelease:
      // case falls through
    case WebUITabStripOpenCloseReason::kOther:
      return gfx::Tween::FAST_OUT_SLOW_IN;
    case WebUITabStripOpenCloseReason::kFling:
      return gfx::Tween::LINEAR_OUT_SLOW_IN;
  }
}

// Returns the base duration of the animation used to open or close the
// tabstrip, before changes are made for shade positioning, gesture velocity,
// etc.
base::TimeDelta GetBaseTabstripOpenCloseAnimationDuration(
    WebUITabStripDragDirection direction) {
  // These values were determined by UX; in the future we may want to change
  // values for fling animations to be consistent for both open and close
  // gestures.
  constexpr base::TimeDelta kHideAnimationDuration =
      base::TimeDelta::FromMilliseconds(200);
  constexpr base::TimeDelta kShowAnimationDuration =
      base::TimeDelta::FromMilliseconds(250);
  switch (direction) {
    case WebUITabStripDragDirection::kUp:
      return kHideAnimationDuration;
    case WebUITabStripDragDirection::kDown:
      return kShowAnimationDuration;
  }
}

// Returns the actual duration of the animation used to open or close the
// tabstrip based on open/close reason, movement direction, and the current
// position of the toolbar.
base::TimeDelta GetTimeDeltaForTabstripOpenClose(
    WebUITabStripOpenCloseReason reason,
    WebUITabStripDragDirection direction,
    double percent_remaining) {
  base::TimeDelta duration =
      GetBaseTabstripOpenCloseAnimationDuration(direction);

  // Fling gestures get shortened based on how little space is left for the
  // toolbar to move. Ideally we'd base it on fling velocity instead but (a) the
  // animation is already very fast, and (b) the event reporting around drag vs.
  // fling is not granular enough to give consistent results.
  if (reason == WebUITabStripOpenCloseReason::kFling) {
    constexpr base::TimeDelta kMinimumAnimationDuration =
        base::TimeDelta::FromMilliseconds(75);
    duration =
        std::max(kMinimumAnimationDuration, duration * percent_remaining);
  }

  return duration;
}

// Converts a y-delta to a drag direction.
WebUITabStripDragDirection DragDirectionFromDelta(float delta) {
  DCHECK(delta != 0.0f);
  return delta > 0.0f ? WebUITabStripDragDirection::kDown
                      : WebUITabStripDragDirection::kUp;
}

// Converts a swipe gesture to a drag direction, or none if the swipe is neither
// up nor down.
base::Optional<WebUITabStripDragDirection> DragDirectionFromSwipe(
    const ui::GestureEvent* event) {
  if (event->details().swipe_down())
    return WebUITabStripDragDirection::kDown;
  if (event->details().swipe_up())
    return WebUITabStripDragDirection::kUp;
  return base::nullopt;
}

bool EventTypeCanCloseTabStrip(const ui::EventType& type) {
  switch (type) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
      return true;
    default:
      return false;
  }
}

class WebUITabStripWebView : public views::WebView {
 public:
  explicit WebUITabStripWebView(content::BrowserContext* context)
      : views::WebView(context) {}

  // content::WebContentsDelegate:
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override {
    // TODO(crbug.com/1032592): Prevent dragging across Chromium instances.
    if (data.custom_data.find(base::ASCIIToUTF16(kWebUITabIdDataType)) !=
        data.custom_data.end()) {
      int tab_id;
      bool found_tab_id = base::StringToInt(
          data.custom_data.at(base::ASCIIToUTF16(kWebUITabIdDataType)),
          &tab_id);
      return found_tab_id && extensions::ExtensionTabUtil::GetTabById(
                                 tab_id, GetBrowserContext(), false, nullptr);
    }

    if (data.custom_data.find(base::ASCIIToUTF16(kWebUITabGroupIdDataType)) !=
        data.custom_data.end()) {
      std::string group_id = base::UTF16ToUTF8(
          data.custom_data.at(base::ASCIIToUTF16(kWebUITabGroupIdDataType)));
      Browser* found_browser = tab_strip_ui::GetBrowserWithGroupId(
          Profile::FromBrowserContext(GetBrowserContext()), group_id);
      return found_browser != nullptr;
    }

    return false;
  }
};

}  // namespace

// When enabled, closes the container for taps in either the web content
// area or the Omnibox (both passed in as View arguments).
class WebUITabStripContainerView::AutoCloser : public ui::EventHandler,
                                               public views::ViewObserver {
 public:
  using CloseCallback = base::RepeatingCallback<void(TabStripUICloseAction)>;

  AutoCloser(CloseCallback close_callback,
             views::View* top_container,
             views::View* content_area,
             views::View* omnibox)
      : close_callback_(std::move(close_callback)),
        top_container_(top_container),
        content_area_(content_area),
        omnibox_(omnibox) {
    DCHECK(top_container_);
    DCHECK(content_area_);
    DCHECK(omnibox_);

    view_observer_.Add(content_area_);
    view_observer_.Add(omnibox_);
#if defined(OS_WIN)
    view_observer_.Add(top_container_);
#endif  // defined(OS_WIN)

    // Our observed Widget's NativeView may be destroyed before us. We
    // have no reasonable way of un-registering our pre-target handler
    // from the NativeView while the Widget is destroying. This disables
    // EventHandler's check that it has been removed from all
    // EventTargets.
    DisableCheckTargets();

    content_area_->GetWidget()->GetNativeView()->AddPreTargetHandler(this);
    pretarget_handler_added_ = true;
  }

  ~AutoCloser() override {
    views::Widget* const widget =
        content_area_ ? content_area_->GetWidget() : nullptr;
    if (pretarget_handler_added_ && widget) {
      widget->GetNativeView()->RemovePreTargetHandler(this);
    }
  }

  // Sets whether to inspect events. If not enabled, all events are
  // ignored and passed through as usual.
  void set_enabled(bool enabled) { enabled_ = enabled; }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (!enabled_)
      return;
    if (!event->IsLocatedEvent())
      return;
    ui::LocatedEvent* located_event = event->AsLocatedEvent();

    if (!EventTypeCanCloseTabStrip(located_event->type()))
      return;

    const gfx::Point event_location_in_screen =
        located_event->target()->GetScreenLocation(*located_event);
    if (!content_area_->GetBoundsInScreen().Contains(event_location_in_screen))
      return;

    // The event may intersect both the content area's bounds and the
    // top container's bounds. In this case, the top container is
    // occluding the web content so we shouldn't close. This happens in
    // immersive mode while the top container is revealed. For more info see
    // https://crbug.com/1112028
    if (top_container_->GetBoundsInScreen().Contains(event_location_in_screen))
      return;

    located_event->StopPropagation();
    close_callback_.Run(TabStripUICloseAction::kTapInTabContent);
  }

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override {
    if (observed_view != omnibox_)
      return;
    if (!enabled_)
      return;

    close_callback_.Run(TabStripUICloseAction::kOmniboxFocusedOrNewTabOpened);
  }

  void OnViewIsDeleting(views::View* observed_view) override {
    view_observer_.Remove(observed_view);
    if (observed_view == content_area_)
      content_area_ = nullptr;
    else if (observed_view == omnibox_)
      omnibox_ = nullptr;
    else if (observed_view == top_container_)
      top_container_ = nullptr;
    else
      NOTREACHED();
  }

  void OnViewAddedToWidget(views::View* observed_view) override {
    if (observed_view != content_area_)
      return;
    if (pretarget_handler_added_)
      return;
    aura::Window* const native_view =
        content_area_->GetWidget()->GetNativeView();
    if (native_view)
      native_view->RemovePreTargetHandler(this);
  }

  void OnViewRemovedFromWidget(views::View* observed_view) override {
    if (observed_view != content_area_)
      return;
    aura::Window* const native_view =
        content_area_->GetWidget()->GetNativeView();
    if (native_view)
      native_view->RemovePreTargetHandler(this);
    pretarget_handler_added_ = false;
  }

 private:
  CloseCallback close_callback_;
  views::View* top_container_;
  views::View* content_area_;
  views::View* omnibox_;

  bool enabled_ = false;

  bool pretarget_handler_added_ = false;

  ScopedObserver<views::View, views::ViewObserver> view_observer_{this};
};

class WebUITabStripContainerView::DragToOpenHandler : public ui::EventHandler {
 public:
  DragToOpenHandler(WebUITabStripContainerView* container,
                    views::View* drag_handle)
      : container_(container), drag_handle_(drag_handle) {
    DCHECK(container_);
    drag_handle_->AddPreTargetHandler(this);
  }

  ~DragToOpenHandler() override { drag_handle_->RemovePreTargetHandler(this); }

  // Cancels any current drag.
  void CancelDrag() { drag_in_progress_ = false; }

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN: {
        // Only treat this scroll as drag-to-open if the y component is
        // larger. Otherwise, leave the event unhandled. Horizontal
        // scrolls are used in the toolbar, e.g. for text scrolling in
        // the Omnibox.
        float y_delta = event->details().scroll_y_hint();
        if (std::fabs(y_delta) > std::fabs(event->details().scroll_x_hint()) &&
            container_->CanStartDragToOpen(DragDirectionFromDelta(y_delta))) {
          drag_in_progress_ = true;
          container_->UpdateHeightForDragToOpen(y_delta);
          event->SetHandled();
        }
        break;
      }
      case ui::ET_GESTURE_SCROLL_UPDATE:
        if (drag_in_progress_) {
          container_->UpdateHeightForDragToOpen(event->details().scroll_y());
          event->SetHandled();
        }
        break;
      case ui::ET_GESTURE_SCROLL_END:
        if (drag_in_progress_) {
          container_->EndDragToOpen();
          event->SetHandled();
          drag_in_progress_ = false;
        }
        break;
      case ui::ET_GESTURE_SWIPE: {
        // If a touch is released at high velocity, the scroll gesture
        // is "converted" to a swipe gesture. ET_GESTURE_END is still
        // sent after. From logging, it seems like ET_GESTURE_SCROLL_END
        // is sometimes also sent after this. It will be ignored here
        // since |drag_in_progress_| is set to false.
        const auto direction = DragDirectionFromSwipe(event);

        // If a swipe happens quickly enough, scroll events might not have
        // been sent, so we may have to start one.
        if (!drag_in_progress_) {
          if (!direction.has_value() ||
              !container_->CanStartDragToOpen(direction.value())) {
            break;
          }
          container_->UpdateHeightForDragToOpen(0.0f);
        }

        // If there is a direction, then end the drag with a fling, otherwise
        // (in the case of a sideways fling) use the default release logic.
        container_->EndDragToOpen(direction);

        event->SetHandled();
        drag_in_progress_ = false;
      } break;
      case ui::ET_GESTURE_END:
        if (drag_in_progress_) {
          // If an unsupported gesture is sent, ensure that we still
          // finish the drag on gesture end. Otherwise, the container
          // will be stuck partially open.
          container_->EndDragToOpen();
          event->SetHandled();
          drag_in_progress_ = false;
        }
        break;
      default:
        break;
    }
  }

 private:
  WebUITabStripContainerView* const container_;
  views::View* const drag_handle_;

  bool drag_in_progress_ = false;
};

WebUITabStripContainerView::WebUITabStripContainerView(
    BrowserView* browser_view,
    views::View* tab_contents_container,
    views::View* top_container,
    views::View* omnibox)
    : browser_view_(browser_view),
      web_view_(AddChildView(
          std::make_unique<WebUITabStripWebView>(browser_view_->GetProfile()))),
      top_container_(top_container),
      tab_contents_container_(tab_contents_container),
      auto_closer_(std::make_unique<AutoCloser>(
          base::Bind(&WebUITabStripContainerView::CloseForEventOutsideTabStrip,
                     base::Unretained(this)),
          browser_view->top_container(),
          tab_contents_container,
          omnibox)),
      drag_to_open_handler_(
          std::make_unique<DragToOpenHandler>(this, top_container)) {
  TRACE_EVENT0("ui", "WebUITabStripContainerView.Init");
  DCHECK(UseTouchableTabStrip(browser_view_->browser()));

  SetVisible(false);
  animation_.Reset(0.0);

  // TODO(crbug.com/1010589) WebContents are initially assumed to be visible by
  // default unless explicitly hidden. The WebContents need to be set to hidden
  // so that the visibility state of the document in JavaScript is correctly
  // initially set to 'hidden', and the 'visibilitychange' events correctly get
  // fired.
  web_view_->GetWebContents()->WasHidden();

  web_view_->set_allow_accelerators(true);

  // Use a vertical flex layout with cross-axis set to stretch. This allows us
  // to add e.g. a hidden title bar, header, footer, etc. by just adding child
  // views.
  auto* const layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  web_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(base::BindRepeating(
          &WebUITabStripContainerView::FlexRule, base::Unretained(this))));

  web_view_->LoadInitialURL(GURL(chrome::kChromeUITabStripURL));
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view_->web_contents());
  task_manager::WebContentsTags::CreateForTabContents(
      web_view_->web_contents());

  DCHECK(tab_contents_container);
  view_observer_.Add(tab_contents_container_);
  view_observer_.Add(top_container_);

  TabStripUI* const tab_strip_ui = static_cast<TabStripUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  tab_strip_ui->Initialize(browser_view_->browser(), this);
}

WebUITabStripContainerView::~WebUITabStripContainerView() {
  // The TabCounter button uses |this| as a listener. We need to make
  // sure we outlive it.
  delete tab_counter_;
}

// static
bool WebUITabStripContainerView::SupportsTouchableTabStrip(
    const Browser* browser) {
  return browser->is_type_normal() &&
         base::FeatureList::IsEnabled(features::kWebUITabStrip);
}

// static
bool WebUITabStripContainerView::UseTouchableTabStrip(const Browser* browser) {
  // This is called at Browser start to check which mode to use. It is a
  // good place to check the feature state and set up a synthetic field
  // trial.
  WebUITabStripFieldTrial::RegisterFieldTrialIfNecessary();

  return browser->is_type_normal() &&
         base::FeatureList::IsEnabled(features::kWebUITabStrip) &&
         ui::TouchUiController::Get()->touch_ui();
}

// static
void WebUITabStripContainerView::GetDropFormatsForView(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats |= ui::OSExchangeData::PICKLED_DATA;
  format_types->insert(ui::ClipboardFormatType::GetWebCustomDataType());
}

// static
bool WebUITabStripContainerView::IsDraggedTab(const ui::OSExchangeData& data) {
  base::Pickle pickle;
  if (data.GetPickledData(ui::ClipboardFormatType::GetWebCustomDataType(),
                          &pickle)) {
    base::string16 result;
    ui::ReadCustomDataForType(pickle.data(), pickle.size(),
                              base::ASCIIToUTF16(kWebUITabIdDataType), &result);
    if (result.size())
      return true;
    ui::ReadCustomDataForType(pickle.data(), pickle.size(),
                              base::ASCIIToUTF16(kWebUITabGroupIdDataType),
                              &result);
    if (result.size())
      return true;
  }

  return false;
}

void WebUITabStripContainerView::OpenForTabDrag() {
  if (GetVisible() && !animation_.IsClosing())
    return;

  RecordTabStripUIOpenHistogram(TabStripUIOpenAction::kTabDraggedIntoWindow);
  SetContainerTargetVisibility(true, WebUITabStripOpenCloseReason::kOther);
}

views::NativeViewHost* WebUITabStripContainerView::GetNativeViewHost() {
  return web_view_->holder();
}

std::unique_ptr<views::View> WebUITabStripContainerView::CreateTabCounter() {
  DCHECK_EQ(nullptr, tab_counter_);

  auto tab_counter = CreateWebUITabCounterButton(
      base::BindRepeating(&WebUITabStripContainerView::TabCounterPressed,
                          base::Unretained(this)),
      browser_view_);

  tab_counter_ = tab_counter.get();
  view_observer_.Add(tab_counter_);

  return tab_counter;
}

void WebUITabStripContainerView::SetVisibleForTesting(bool visible) {
  SetContainerTargetVisibility(visible, WebUITabStripOpenCloseReason::kOther);
  FinishAnimationForTesting();
}

void WebUITabStripContainerView::FinishAnimationForTesting() {
  if (!animation_.is_animating())
    return;
  const bool target = animation_.IsShowing();
  animation_.SetCurrentValue(target ? 1.0 : 0.0);
  animation_.End();
  PreferredSizeChanged();
}

const ui::AcceleratorProvider*
WebUITabStripContainerView::GetAcceleratorProvider() const {
  return browser_view_;
}

void WebUITabStripContainerView::CloseContainer() {
  SetContainerTargetVisibility(false, WebUITabStripOpenCloseReason::kOther);
  browser_view_->feature_promo_controller()
      ->feature_engagement_tracker()
      ->NotifyEvent(feature_engagement::events::kWebUITabStripClosed);
}

bool WebUITabStripContainerView::CanStartDragToOpen(
    WebUITabStripDragDirection direction) const {
  // If we're already in a drag, then we can always continue dragging.
  if (current_drag_height_)
    return true;
  return direction == (GetVisible() ? WebUITabStripDragDirection::kUp
                                    : WebUITabStripDragDirection::kDown);
}

void WebUITabStripContainerView::UpdateHeightForDragToOpen(float height_delta) {
  if (!current_drag_height_) {
    const bool was_open = GetVisible();
    DCHECK(!was_open || height_delta <= 0.0f);
    DCHECK(was_open || height_delta >= 0.0f);

    SetVisible(true);
    current_drag_height_ = was_open ? height() : 0.0f;
    animation_.Reset();
  }

  current_drag_height_ =
      base::ClampToRange(*current_drag_height_ + height_delta, 0.0f,
                         static_cast<float>(GetPreferredSize().height()));
  PreferredSizeChanged();
}

void WebUITabStripContainerView::EndDragToOpen(
    base::Optional<WebUITabStripDragDirection> fling_direction) {
  if (!current_drag_height_)
    return;

  const int final_drag_height = *current_drag_height_;
  current_drag_height_ = base::nullopt;

  // If this wasn't a fling, determine whether to open or close based on
  // final height.
  const double open_proportion =
      static_cast<double>(final_drag_height) / GetPreferredSize().height();
  bool opening = open_proportion >= 0.5;
  if (fling_direction) {
    // If this was a fling, ignore the final height and use the fling
    // direction.
    opening = (fling_direction == WebUITabStripDragDirection::kDown);
  }

  if (opening) {
    RecordTabStripUIOpenHistogram(TabStripUIOpenAction::kToolbarDrag);
    browser_view_->feature_promo_controller()
        ->feature_engagement_tracker()
        ->NotifyEvent(feature_engagement::events::kWebUITabStripOpened);
  } else {
    browser_view_->feature_promo_controller()
        ->feature_engagement_tracker()
        ->NotifyEvent(feature_engagement::events::kWebUITabStripClosed);
  }

  animation_.Reset(open_proportion);
  SetContainerTargetVisibility(
      opening, fling_direction.has_value()
                   ? WebUITabStripOpenCloseReason::kFling
                   : WebUITabStripOpenCloseReason::kDragRelease);
}

void WebUITabStripContainerView::TabCounterPressed(const ui::Event& event) {
  const bool new_visibility = !GetVisible();
  if (new_visibility) {
    RecordTabStripUIOpenHistogram(TabStripUIOpenAction::kTapOnTabCounter);
    browser_view_->feature_promo_controller()
        ->feature_engagement_tracker()
        ->NotifyEvent(feature_engagement::events::kWebUITabStripOpened);
  } else {
    RecordTabStripUICloseHistogram(TabStripUICloseAction::kTapOnTabCounter);
    browser_view_->feature_promo_controller()
        ->feature_engagement_tracker()
        ->NotifyEvent(feature_engagement::events::kWebUITabStripClosed);
  }

  SetContainerTargetVisibility(new_visibility,
                               WebUITabStripOpenCloseReason::kOther);

  if (GetVisible() && event.IsKeyEvent()) {
    // Automatically move focus to the tab strip WebUI if the tab strip
    // was opened via a key event.
    SetPaneFocusAndFocusDefault();
  }
}

void WebUITabStripContainerView::SetContainerTargetVisibility(
    bool target_visible,
    WebUITabStripOpenCloseReason reason) {
  if (target_visible) {
    immersive_revealed_lock_.reset(
        browser_view_->immersive_mode_controller()->GetRevealedLock(
            ImmersiveModeController::ANIMATE_REVEAL_YES));

    SetVisible(true);
    PreferredSizeChanged();
    const double current_value = animation_.GetCurrentValue();
    if (current_value < 1.0) {
      animation_.SetSlideDuration(GetTimeDeltaForTabstripOpenClose(
          reason, WebUITabStripDragDirection::kDown,
          /* percent_remaining */ 1.0 - current_value));
      animation_.SetTweenType(GetTweenTypeForTabstripOpenClose(reason));
      animation_.Show();
    }

    // Switch focus to the WebView container. This prevents a confusing
    // situation where a View appears to have focus, but keyboard inputs
    // are actually directed to the WebUITabStrip.
    web_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    web_view_->RequestFocus();

    time_at_open_ = base::TimeTicks::Now();

    if (browser_view_->feature_promo_controller()->BubbleIsShowing(
            feature_engagement::kIPHWebUITabStripFeature)) {
      browser_view_->feature_promo_controller()->CloseBubble(
          feature_engagement::kIPHWebUITabStripFeature);
    }
  } else {
    if (time_at_open_) {
      RecordTabStripUIOpenDurationHistogram(base::TimeTicks::Now() -
                                            time_at_open_.value());
      time_at_open_ = base::nullopt;
    }

    const double current_value = animation_.GetCurrentValue();
    if (current_value > 0.0) {
      animation_.SetSlideDuration(GetTimeDeltaForTabstripOpenClose(
          reason, WebUITabStripDragDirection::kUp,
          /* percent_remaining */ current_value));
      animation_.SetTweenType(GetTweenTypeForTabstripOpenClose(reason));
      animation_.Hide();
    } else {
      PreferredSizeChanged();
      SetVisible(false);
    }

    web_view_->SetFocusBehavior(FocusBehavior::NEVER);

    immersive_revealed_lock_.reset();
  }
  auto_closer_->set_enabled(target_visible);
}

void WebUITabStripContainerView::CloseForEventOutsideTabStrip(
    TabStripUICloseAction reason) {
  RecordTabStripUICloseHistogram(reason);
  SetContainerTargetVisibility(false, WebUITabStripOpenCloseReason::kOther);
}

void WebUITabStripContainerView::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK_EQ(&animation_, animation);
  PreferredSizeChanged();
  if (animation_.GetCurrentValue() == 0.0)
    SetVisible(false);
}

void WebUITabStripContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void WebUITabStripContainerView::ShowContextMenuAtPoint(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  ConvertPointToScreen(this, &point);
  context_menu_model_ = std::move(menu_model);
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);
}

void WebUITabStripContainerView::ShowEditDialogForGroupAtPoint(
    gfx::Point point,
    gfx::Rect rect,
    tab_groups::TabGroupId group) {
  ConvertPointToScreen(this, &point);
  rect.set_origin(point);
  TabGroupEditorBubbleView::Show(browser_view_->browser(), group, nullptr, rect,
                                 this);
}

TabStripUILayout WebUITabStripContainerView::GetLayout() {
  DCHECK(tab_contents_container_);
  return TabStripUILayout::CalculateForWebViewportSize(
      tab_contents_container_->size());
}

SkColor WebUITabStripContainerView::GetColor(int id) const {
  return GetThemeProvider()->GetColor(id);
}

int WebUITabStripContainerView::GetHeightForWidth(int w) const {
  DCHECK(!(animation_.is_animating() && current_drag_height_));

  // Note that preferred size is automatically calculated by the layout.
  if (animation_.is_animating()) {
    return gfx::Tween::LinearIntValueBetween(animation_.GetCurrentValue(), 0,
                                             GetPreferredSize().height());
  }
  if (current_drag_height_)
    return std::round(*current_drag_height_);

  return GetVisible() ? GetPreferredSize().height() : 0;
}

gfx::Size WebUITabStripContainerView::FlexRule(
    const views::View* view,
    const views::SizeBounds& bounds) const {
  DCHECK_EQ(view, web_view_);
  const int width = bounds.width().is_bounded()
                        ? bounds.width().value()
                        : tab_contents_container_->width();
  const int height = TabStripUILayout::CalculateForWebViewportSize(
                         tab_contents_container_->size())
                         .CalculateContainerHeight();

  return gfx::Size(width, height);
}

void WebUITabStripContainerView::OnViewBoundsChanged(View* observed_view) {
#if defined(OS_WIN)
  if (observed_view == top_container_) {
    if (old_top_container_width_ != top_container_->width()) {
      old_top_container_width_ = top_container_->width();
      // If somehow we're in the middle of a drag, abort.
      drag_to_open_handler_->CancelDrag();
      CloseContainer();
    }
    return;
  }
#endif  // defined(OS_WIN)

  if (observed_view == tab_contents_container_) {
    // TODO(pbos): PreferredSizeChanged seems to cause infinite recursion with
    // BrowserView::ChildPreferredSizeChanged. InvalidateLayout here should be
    // replaceable with PreferredSizeChanged.
    InvalidateLayout();

    TabStripUI* const tab_strip_ui = static_cast<TabStripUI*>(
        web_view_->GetWebContents()->GetWebUI()->GetController());
    tab_strip_ui->LayoutChanged();
  }
}

void WebUITabStripContainerView::OnViewIsDeleting(View* observed_view) {
  view_observer_.Remove(observed_view);

  if (observed_view == tab_counter_)
    tab_counter_ = nullptr;
  else if (observed_view == tab_contents_container_)
    tab_contents_container_ = nullptr;
}

bool WebUITabStripContainerView::SetPaneFocusAndFocusDefault() {
  // Make sure the pane first receives focus, then send a WebUI event to the
  // front-end so the correct HTML element receives focus.
  bool received_focus = AccessiblePaneView::SetPaneFocusAndFocusDefault();
  if (received_focus) {
    TabStripUI* const tab_strip_ui = static_cast<TabStripUI*>(
        web_view_->GetWebContents()->GetWebUI()->GetController());
    tab_strip_ui->ReceivedKeyboardFocus();
  }
  return received_focus;
}
