// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

OmniboxPopupFullPresenter::OmniboxPopupFullPresenter(
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate,
    OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar, presenter_delegate, controller) {
  // `location_bar` may be null in unit tests.
  if (location_bar) {
    SetWebUIContent(std::make_unique<OmniboxFullPopupWebUIContent>(
        this, this->location_bar(), controller));
  }
}

OmniboxPopupFullPresenter::~OmniboxPopupFullPresenter() = default;

void OmniboxPopupFullPresenter::Show() {
  const bool was_shown = IsShown();
  OmniboxPopupPresenterBase::Show();
  if (!was_shown) {
    // Set the request time to now when the popup is first shown. This ensures
    // that latency is measured from the user interaction to show, even if the
    // WebUI was preloaded at startup.
    WebUIContentsPreloadManager::GetInstance()->SetRequestTime(
        GetWebUIContent()->GetWebContents(), base::TimeTicks::Now());

    if (!logged_first_shown_metric_) {
      if (auto* popup_view = location_bar()->GetOmniboxPopupView()) {
        const base::TimeDelta delta =
            base::TimeTicks::Now() - popup_view->construction_time();
        logged_first_shown_metric_ = true;
        base::UmaHistogramTimes(
            base::StrCat(
                {GetPopupMetricPrefix(), ".ConstructionToFirstShownDuration"}),
            delta);
      }
    }

    // Forward events for a short period of time so that double clicks on the
    // omnibox can still be captured.
    if (GetWidget() && base::FeatureList::IsEnabled(
                           omnibox::kWebUIOmniboxFullPopupDoubleClick)) {
      auto* results_frame =
          views::AsViewClass<FullWebUIOmniboxFrame>(GetResultsFrame());
      CHECK(results_frame);
      results_frame->SetForwardMouseEvents(true);
      forward_events_timer_.Start(
          FROM_HERE, base::Milliseconds(500),
          base::BindOnce(&OmniboxPopupFullPresenter::StopForwardingEvents,
                         base::Unretained(this)));
    }
  }

  auto* controller =
      GetWebUIContent()->contents_wrapper()->GetWebUIController();
  auto* handler = controller ? controller->omnibox_handler() : nullptr;
  auto* omnibox_view = location_bar()->GetOmniboxView();
  if (handler && omnibox_view) {
    handler->SetAimButtonVisible(omnibox_view->AimButtonVisible());
  }

  views::Widget* parent_widget = delegate().GetLocationBarWidget();
  if (parent_widget && !parent_widget_observation_.IsObserving()) {
    parent_widget_observation_.Observe(parent_widget);
  }

  if (GetWidget() && !popup_widget_observation_.IsObserving()) {
    popup_widget_observation_.Observe(GetWidget());
  }

  if (parent_widget && !event_monitor_) {
    event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
        this, parent_widget->GetNativeWindow(), {ui::EventType::kMousePressed});
  }
}

void OmniboxPopupFullPresenter::Hide() {
  parent_widget_observation_.Reset();
  event_monitor_.reset();
  forward_events_timer_.Stop();
  popup_widget_observation_.Reset();
  OmniboxPopupPresenterBase::Hide();
  if (ShouldApplyHeightWorkarounds()) {
    // Reset the cached height to force a layout update when the popup is
    // reshown. This prevents the popup from temporarily using a stale size
    // from its previous state.
    content_height_ = 1;
  }
}

void OmniboxPopupFullPresenter::RequestFocus() {
  if (GetWidget() && ShouldReceiveFocus()) {
    if (!GetWidget()->IsActive()) {
      GetWidget()->Activate();
      if (GetUIContainer() && GetUIContainer()->GetWidget()) {
        if (auto* focus_manager =
                GetUIContainer()->GetWidget()->GetFocusManager()) {
          // Set stored focus on the container widget to the WebUI content view
          // so that activating the popup widget restores focus to it.
          focus_manager->SetStoredFocusView(GetWebUIContent());
        }
      }
    }
  }
  OmniboxPopupPresenterBase::RequestFocus();
}

std::string_view OmniboxPopupFullPresenter::GetPopupMetricPrefix() const {
  return OmniboxPopupPresenterBase::kFullWebUIPopupMetricPrefix;
}

std::optional<base::TimeDelta>
OmniboxPopupFullPresenter::ShouldDeferUntilVisualStateReady() const {
  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxFullWebUIDeferShowUntilVisualStateReady)) {
    return std::nullopt;
  }
  return base::Milliseconds(
      omnibox::kOmniboxFullWebUIDeferShowUntilVisualStateReadyTimeoutMs.Get());
}

bool OmniboxPopupFullPresenter::ShouldDebounceResize() const {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxFullWebUIDebounceResize);
}

bool OmniboxPopupFullPresenter::ShouldApplyHeightWorkarounds() const {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxFullWebUIHeightWorkarounds);
}

bool OmniboxPopupFullPresenter::ShouldDetachWebContentsOnHide() const {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxFullWebUIDetachWebContentsOnHide);
}

bool OmniboxPopupFullPresenter::ShouldEvictOnHide() const {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxFullWebUIEvictOnHide);
}

bool OmniboxPopupFullPresenter::ShouldSizeWebViewToPreferredHeight() const {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxFullWebUISizeWebViewToPreferredHeight);
}

bool OmniboxPopupFullPresenter::ShouldHideForInitialLayout() const {
  return false;
}

std::unique_ptr<RoundedOmniboxResultsFrame>
OmniboxPopupFullPresenter::CreateResultsFrame(
    std::unique_ptr<views::View> contents,
    LocationBar* location_bar,
    bool forward_mouse_events) {
  return std::make_unique<FullWebUIOmniboxFrame>(
      contents.release(), location_bar, forward_mouse_events);
}

bool OmniboxPopupFullPresenter::ShouldPreserveRequestedFocus() const {
  return true;
}

bool OmniboxPopupFullPresenter::IsDeactivating() const {
  return is_deactivating_;
}

void OmniboxPopupFullPresenter::SynchronizePopupBounds() {
  if (!GetWidget()) {
    return;
  }
  // In unit tests, `location_bar` may be null.
  if (!location_bar()) {
    gfx::Rect widget_bounds = GetWidget()->GetRestoredBounds();
    widget_bounds.set_width(
        std::max(get_minimum_size().width(), widget_bounds.width()));
    widget_bounds.set_height(
        std::max(get_minimum_size().height(), widget_bounds.height()));
    GetWidget()->SetBounds(widget_bounds);
    return;
  }

  // Calculate the bounds of the "content area" which includes the location bar
  // and any results, plus the alignment insets to cover the focus ring.
  gfx::Rect widget_bounds = location_bar()->BoundsInScreen();
  widget_bounds.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());

  const int default_height = widget_bounds.height();
  bool has_results = content_height_ > default_height;
  int target_elevation =
      has_results ? RoundedOmniboxResultsFrame::kDefaultElevation : 0;

  auto* results_frame =
      views::AsViewClass<FullWebUIOmniboxFrame>(GetResultsFrame());
  CHECK(results_frame);
  results_frame->SetElevation(target_elevation);

  widget_bounds.set_height(content_height_ > 1 ? content_height_
                                               : default_height);

  // Set width and height to at least their minimums (e.g. for permission
  // prompts).
  widget_bounds.set_width(
      std::max(get_minimum_size().width(), widget_bounds.width()));
  widget_bounds.set_height(
      std::max(get_minimum_size().height(), widget_bounds.height()));

  widget_bounds.Inset(-results_frame->GetInsets());
  GetWidget()->SetBounds(widget_bounds);
}

void OmniboxPopupFullPresenter::NotifyEscapeKeyPressed() {
  is_handling_escape_key_ = true;
}

void OmniboxPopupFullPresenter::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  // If the widget that changed is the browser window.
  if (widget == delegate().GetLocationBarWidget()) {
    if (!active) {
      // When the browser window deactivates (e.g. due to focusing the WebUI
      // popup or opening a bubble), we must cache the Omnibox view
      // as the stored focus view. This ensures that when the browser window
      // reactivates, the FocusManager restores focus to the Omnibox.
      //
      // We must post this as a task because on macOS, native deactivation
      // events run *before* the FocusManager processes
      // `StoreFocusedView(false)`. If we set the stored focus view
      // synchronously, it would immediately get overwritten and clobbered by
      // the FocusManager caching `nullptr` or the `ContentsWebView` during the
      // remainder of the deactivation cycle.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<OmniboxPopupFullPresenter> presenter) {
                if (presenter) {
                  if (auto* widget =
                          presenter->delegate().GetLocationBarWidget()) {
                    if (auto* focus_manager = widget->GetFocusManager()) {
                      focus_manager->SetStoredFocusView(
                          presenter->delegate()
                              .GetLocationBarFocusRestoreView());
                    }
                  }
                }
              },
              weak_factory_.GetWeakPtr()));
    }
    return;
  }

  if (widget == GetWidget()) {
    if (active) {
      OnWidgetActivated();
      return;
    }

    const bool is_esc = is_handling_escape_key_;
    is_handling_escape_key_ = false;
    const bool is_popup_open =
        controller()->popup_state_manager()->popup_state() ==
        OmniboxPopupState::kFull;
    // If deactivation was triggered by an Escape key press while the popup is
    // open, re-request focus asynchronously instead of closing the popup.
    if (is_esc && is_popup_open) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&OmniboxPopupFullPresenter::RequestFocus,
                                    weak_factory_.GetWeakPtr()));
      return;
    }

#if BUILDFLAG(IS_MAC)
    if (IsShown()) {
      // When the suggestions popup widget loses activation on macOS, Cocoa
      // deactivates the child window's view hierarchy, causing the underlying
      // RenderWidgetHostViewMac to automatically set itself to inactive (hiding
      // the selection/caret). Force it to remain active so that the WebUI
      // content continues to paint and respond to events correctly (e.g.
      // showing the caret after clicking the toolbar). On Aura (Windows/Linux),
      // child widgets are managed under a unified focus controller and remain
      // active automatically as long as the browser window is active, so this
      // is only needed on macOS.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<OmniboxPopupFullPresenter> presenter) {
                if (presenter && presenter->IsShown()) {
                  auto* webui_content = presenter->GetWebUIContent();
                  if (webui_content) {
                    content::WebContents* wc =
                        webui_content->GetWrappedWebContents();
                    if (wc && wc->GetRenderWidgetHostView()) {
                      wc->GetRenderWidgetHostView()->SetActive(true);
                    }
                  }
                }
              },
              weak_factory_.GetWeakPtr()));
    }
#endif  // BUILDFLAG(IS_MAC)
  }
}

void OmniboxPopupFullPresenter::DeactivatePopupAndKillFocus() {
  ResetPermissionPromptShowingState();
  is_deactivating_ = true;
  const bool user_input_in_progress =
      controller()->edit_model()->user_input_in_progress();
  const std::u16string& user_text = controller()->edit_model()->user_text();
  const std::u16string permanent_text =
      controller()->edit_model()->GetPermanentDisplayText();
  const std::u16string full_url = controller()->client()->GetFormattedFullURL();

  // If the view is showing text that's not user-text, revert the text to the
  // permanent display text. This usually occurs if Steady State Elisions is on
  // and the user has unelided, but not edited the URL.
  // Also revert if the text has been edited but currently exactly matches
  // the permanent text. An example of this scenario is someone typing on the
  // new tab page and then deleting everything using backspace/delete.
  const bool should_revert_non_user_text =
      !user_input_in_progress && user_text != permanent_text;
  const bool should_revert_matching_text =
      user_input_in_progress &&
      (user_text == permanent_text || user_text == full_url);

  if (should_revert_non_user_text || should_revert_matching_text) {
    controller()->edit_model()->Revert();
  }

  if (auto* popup_view = location_bar()->GetOmniboxPopupView()) {
    popup_view->OnBlur();
  }

  views::Widget* parent_widget = delegate().GetLocationBarWidget();
  if (parent_widget && parent_widget->GetFocusManager()) {
    parent_widget->GetFocusManager()->ClearFocus();
  }

  controller()->client()->FocusWebContents();
  controller()->edit_model()->OnKillFocus();

  // If the user is currently typing do not close the popup.
  if (!user_input_in_progress || user_text.empty()) {
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }

  is_deactivating_ = false;
}

void OmniboxPopupFullPresenter::WidgetDestroyed() {
  event_monitor_.reset();
  forward_events_timer_.Stop();
  popup_widget_observation_.Reset();
  // Update the popup state manager if widget was destroyed externally, e.g., by
  // the OS. This ensures the popup state manager stays in sync.
  if (controller()->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kFull) {
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

void OmniboxPopupFullPresenter::StopForwardingEvents() {
  if (GetWidget()) {
    auto* results_frame =
        views::AsViewClass<FullWebUIOmniboxFrame>(GetResultsFrame());
    CHECK(results_frame);
    results_frame->SetForwardMouseEvents(false);
  }
}

void OmniboxPopupFullPresenter::OnEvent(const ui::Event& event) {
  // TODO(b/543851644): Figure out how to handle touch events.
  if (!event.IsMouseEvent()) {
    return;
  }
  const ui::MouseEvent* mouse_event = event.AsMouseEvent();
  if (mouse_event->type() != ui::EventType::kMousePressed) {
    return;
  }

  // Right-clicks (context menu triggers) should never clear focus.
  if (mouse_event->IsRightMouseButton()) {
    return;
  }

  views::Widget* parent_widget = delegate().GetLocationBarWidget();
  if (!parent_widget) {
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      parent_widget->GetNativeWindow());
  if (!browser_view) {
    return;
  }

  gfx::Point cursor_point = display::Screen::Get()->GetCursorScreenPoint();
  bool contains_top_container = false;

  if (browser_view->top_container()) {
    gfx::Rect top_container_bounds =
        browser_view->top_container()->GetBoundsInScreen();
    gfx::Rect window_bounds = parent_widget->GetWindowBoundsInScreen();
    contains_top_container = cursor_point.x() >= window_bounds.x() &&
                             cursor_point.x() < window_bounds.right() &&
                             cursor_point.y() >= window_bounds.y() &&
                             cursor_point.y() < top_container_bounds.bottom();
  }

  bool contains_popup = false;
  if (IsShown()) {
    contains_popup =
        GetWidget()->GetWindowBoundsInScreen().Contains(cursor_point);
  }

  if (contains_popup) {
    return;
  }

  // Clear autocomplete matches and reset activeQueryId_ on WebUI only if
  // click is outside of the popup and the popup is shown.
  if (IsShown() && GetWebUIContent() && GetWebUIContent()->contents_wrapper()) {
    if (auto* webui_controller =
            GetWebUIContent()->contents_wrapper()->GetWebUIController()) {
      if (auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller)) {
        if (auto* popup_handler = popup_ui->popup_handler()) {
          popup_handler->ClearAutocompleteMatches();
        }
      }
    }
  }

  if (contains_top_container) {
    return;
  }

  DeactivatePopupAndKillFocus();
}
