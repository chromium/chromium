// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
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

#if BUILDFLAG(IS_LINUX)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

// Returns whether the browser window, including child widgets such as the
// popup and bubbles, is active. On Aura, the popup can keep Aura activation,
// and the browser's paint-as-active state, even after the browser window
// deactivates, so ask the platform or window manager directly. If that lookup
// fails, the window is being torn down or is not fully set up, so treat it as
// inactive rather than trusting paint-as-active.
bool IsBrowserWindowActive(views::Widget* browser_widget) {
  if (!browser_widget) {
    return false;
  }
#if BUILDFLAG(IS_LINUX)
  if (aura::Window* native_window = browser_widget->GetNativeWindow();
      native_window && native_window->GetHost()) {
    if (auto* host = views::DesktopWindowTreeHostPlatform::GetHostForWidget(
            native_window->GetHost()->GetAcceleratedWidget())) {
      return host->IsActive();
    }
  }
  return false;
#elif BUILDFLAG(IS_WIN)
  if (HWND hwnd = views::HWNDForWidget(browser_widget)) {
    // Windows owned by the browser window (e.g. bubbles) count as active.
    HWND active_hwnd = ::GetActiveWindow();
    return active_hwnd && (active_hwnd == hwnd ||
                           ::GetAncestor(active_hwnd, GA_ROOTOWNER) == hwnd);
  }
  return false;
#elif BUILDFLAG(IS_CHROMEOS)
  // On Ash, every window shares one activation client, and activation moves
  // between windows in a single step, so ask it directly. Paint-as-active
  // briefly reports inactive during activation handoffs within the browser
  // window (e.g. from the popup to the tab strip). The popup and bubbles are
  // transient children of the browser window, so they count as active.
  if (aura::Window* native_window = browser_widget->GetNativeWindow()) {
    if (wm::ActivationClient* activation_client =
            wm::GetActivationClient(native_window->GetRootWindow())) {
      aura::Window* active_window = activation_client->GetActiveWindow();
      return active_window &&
             native_window->Contains(wm::GetTransientRoot(active_window));
    }
  }
  return false;
#else
  // Mac (and any other platform) has no Aura activation mismatch, so
  // paint-as-active tracks the browser window's activation.
  return browser_widget->ShouldPaintAsActive();
#endif
}

}  // namespace

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
      if (auto* results_frame =
              views::AsViewClass<FullWebUIOmniboxFrame>(GetResultsFrame())) {
        results_frame->SetForwardMouseEvents(true);
        forward_events_timer_.Start(
            FROM_HERE, base::Milliseconds(500),
            base::BindOnce(&OmniboxPopupFullPresenter::StopForwardingEvents,
                           base::Unretained(this)));
      }
    }
  }

  auto* controller =
      GetWebUIContent()->contents_wrapper()->GetWebUIController();
  auto* handler = controller ? controller->omnibox_handler() : nullptr;
  auto* omnibox_view = location_bar()->GetOmniboxView();
  if (handler && omnibox_view) {
    handler->SetAimButtonVisible(omnibox_view->AimButtonVisible());
  }

  views::Widget* browser_widget = GetBrowserWidget();
  if (browser_widget && !browser_widget_observation_.IsObserving()) {
    browser_widget_observation_.Observe(browser_widget);
  }
  if (browser_widget && !browser_paint_as_active_subscription_) {
    // Catches the user leaving the browser window while neither the browser
    // nor the popup widget is active (e.g. while a bubble is active). Posted
    // because the popup releasing its paint-as-active lock on the browser and
    // the browser widget becoming active are separate notifications, so a
    // handoff between them can briefly report inactive.
    //
    // This only triggers the check. `IsBrowserWindowActive()` decides, and
    // outside Mac it asks the platform instead of paint-as-active.
    browser_paint_as_active_subscription_ =
        browser_widget->RegisterPaintAsActiveChangedCallback(
            base::BindPostTaskToCurrentDefault(base::BindRepeating(
                &OmniboxPopupFullPresenter::BlurIfBrowserWindowInactive,
                weak_factory_.GetWeakPtr())));
  }

  if (GetWidget() && !popup_widget_observation_.IsObserving()) {
    popup_widget_observation_.Observe(GetWidget());
  }

  if (browser_widget && !event_monitor_) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, browser_widget->GetNativeWindow(),
        {ui::EventType::kMousePressed});
  }

  if (browser_widget) {
    if (auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
            browser_widget->GetNativeWindow())) {
      if (browser_view->toolbar_button_provider() &&
          browser_view->toolbar_button_provider()->GetAppMenuControl() &&
          !app_menu_control_observation_.IsObserving()) {
        app_menu_control_observation_.Observe(
            browser_view->toolbar_button_provider()->GetAppMenuControl());
      }
    }
  }
}

void OmniboxPopupFullPresenter::Hide() {
  pending_focus_task_.Cancel();
  browser_widget_observation_.Reset();
  browser_paint_as_active_subscription_ = {};
  app_menu_control_observation_.Reset();
  event_monitor_.reset();
  forward_events_timer_.Stop();
  StopForwardingEvents();
  popup_widget_observation_.Reset();
  if (auto* focus_manager = GetBrowserFocusManager()) {
    views::View* restore_view = delegate().GetLocationBarFocusRestoreView();
    views::View* stored_view = focus_manager->GetStoredFocusView();
    if (stored_view == restore_view) {
      focus_manager->SetStoredFocusView(nullptr);
    }
  }
  OmniboxPopupPresenterBase::Hide();
  if (ShouldApplyHeightWorkarounds()) {
    // Reset the cached height to force a layout update when the popup is
    // reshown. This prevents the popup from temporarily using a stale size
    // from its previous state.
    content_height_ = 1;
  }
}

void OmniboxPopupFullPresenter::RequestFocus() {
  if (views::MenuController::GetActiveInstance()) {
    return;
  }
  if (ShouldPreserveRequestedFocus()) {
    focus_requested_ = true;
  }

  // Don't take focus in a browser window the user has left. Activating the
  // popup would pull the window back in front of the one the user switched to.
  if (!GetWidget() || !ShouldReceiveFocus() ||
      (!IsBrowserWindowActive(GetBrowserWidget()))) {
    return;
  }

  // If the popup widget is already active, focus the WebUI content now so the
  // caret and selection are correct within this event loop turn. Do NOT return
  // here: the activation seen at this instant can be torn down later in the
  // same tab-switch sequence, leaving the popup looking focused while keys go
  // to the browser widget. See crbug.com/558761131.
  if (GetWidget()->IsActive()) {
    FocusPopupContent();
  }

  // Defer activation to the next event loop cycle to avoid re-entrant window
  // activation in Aura FocusController during tab switches or startup.
  pending_focus_task_.Reset(base::BindOnce(
      [](base::WeakPtr<OmniboxPopupFullPresenter> presenter) {
        // Activation is deferred asynchronously. Re-verify that the presenter,
        // widget, and popup state remain valid, and that focus was not lost
        // (e.g., user clicked away into the web page or opened a menu) while
        // the task was queued.
        if (!presenter || !presenter->GetWidget() ||
            !presenter->ShouldReceiveFocus() || !presenter->IsShown() ||
            !presenter->focus_requested_ ||
            !presenter->controller()->edit_model()->has_focus() ||
            presenter->controller()->popup_state_manager()->popup_state() !=
                OmniboxPopupState::kFull) {
          return;
        }
        if (views::MenuController::GetActiveInstance()) {
          return;
        }
        // The browser window may have been deactivated while this task was
        // queued (e.g. another window opened).
        if (!IsBrowserWindowActive(presenter->GetBrowserWidget())) {
          return;
        }
        if (auto* focus_manager = presenter->GetWidget()->GetFocusManager()) {
          // Ensure activating the container restores focus to the WebUI view.
          focus_manager->SetStoredFocusView(presenter->GetWebUIContent());
        }
        if (!presenter->GetWidget()->IsActive()) {
          presenter->GetWidget()->Activate();
        }
        presenter->FocusPopupContent();
      },
      weak_factory_.GetWeakPtr()));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, pending_focus_task_.callback());
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

bool OmniboxPopupFullPresenter::ShouldDrawShadowInWebUI() const {
  return omnibox::ShouldDrawFullPopupShadowInWebUI();
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

bool OmniboxPopupFullPresenter::ShouldReceiveFocus() const {
  if (views::MenuController::GetActiveInstance()) {
    return false;
  }
  return OmniboxPopupPresenterBase::ShouldReceiveFocus();
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
  widget_bounds.Inset(-FullWebUIOmniboxFrame::GetLocationBarAlignmentInsets());

  const int default_height = widget_bounds.height();

  auto* results_frame =
      views::AsViewClass<FullWebUIOmniboxFrame>(GetResultsFrame());
  CHECK(results_frame);
  const bool shadow_in_webui = ShouldDrawShadowInWebUI();
  if (!shadow_in_webui) {
    const bool has_results = content_height_ > default_height;
    results_frame->SetElevation(
        has_results ? RoundedOmniboxResultsFrame::kDefaultElevation : 0);
  }

  widget_bounds.set_height(content_height_ > 1 ? content_height_
                                               : default_height);

  // Set width and height to at least their minimums (e.g. for permission
  // prompts).
  widget_bounds.set_width(
      std::max(get_minimum_size().width(), widget_bounds.width()));
  widget_bounds.set_height(
      std::max(get_minimum_size().height(), widget_bounds.height()));

  // Normally the frame's border supplies the shadow margin. When the page
  // paints the shadow the frame has no border, so add the margin here; the
  // WebView covers it and the page paints the shadow into it.
  widget_bounds.Inset(-(shadow_in_webui
                            ? RoundedOmniboxResultsFrame::GetShadowInsets()
                            : results_frame->GetInsets()));
  GetWidget()->SetBounds(widget_bounds);
}

void OmniboxPopupFullPresenter::NotifyEscapeKeyPressed() {
  is_handling_escape_key_ = true;
}

void OmniboxPopupFullPresenter::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  if (!active) {
    // The browser or popup widget deactivating may mean the user left the
    // browser window. Check once activation settles, since handoffs between
    // the browser, the popup, and bubbles deactivate one widget before
    // activating the next. The paint-as-active subscription
    // (`browser_paint_as_active_subscription_`) isn't enough on its own, since
    // on Aura the popup can keep the browser painting as active after the
    // platform window deactivates. See `IsBrowserWindowActive()`.
    //
    // Must be posted before the stored focus view task below, so that the
    // omnibox is blurred by the time that task runs and it does nothing.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&OmniboxPopupFullPresenter::BlurIfBrowserWindowInactive,
                       weak_factory_.GetWeakPtr()));
  }

  // If the widget that changed is the browser window.
  if (widget == GetBrowserWidget()) {
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
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<OmniboxPopupFullPresenter> presenter) {
                if (presenter && presenter->IsShown() &&
                    presenter->controller()->edit_model()->has_focus()) {
                  if (auto* focus_manager =
                          presenter->GetBrowserFocusManager()) {
                    focus_manager->SetStoredFocusView(
                        presenter->delegate().GetLocationBarFocusRestoreView());
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
#if BUILDFLAG(IS_MAC)
      // On macOS, the popup is a separate window that only becomes key when
      // the user returns to it (e.g. swiping back to the Space), so treat that
      // as returning to the omnibox. On Aura, the popup can become active when
      // the browser window deactivates, so its activation isn't a reliable
      // signal.
      if (IsShown() && !controller()->edit_model()->has_focus()) {
        if (auto* popup_view = location_bar()->GetOmniboxPopupView()) {
          popup_view->OnFocus(/*query_zps=*/false);
        }
      }
#endif  // BUILDFLAG(IS_MAC)
      return;
    }

    const bool is_esc = is_handling_escape_key_;
    is_handling_escape_key_ = false;
    const bool is_popup_open =
        controller()->popup_state_manager()->popup_state() ==
        OmniboxPopupState::kFull;
    if (is_esc && is_popup_open) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

void OmniboxPopupFullPresenter::BlurIfBrowserWindowInactive() {
  if (!IsBrowserWindowActive(delegate().GetLocationBarWidget())) {
    BlurForWindowDeactivation();
  }
}

void OmniboxPopupFullPresenter::BlurForWindowDeactivation() {
  if (controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kFull ||
      !controller()->edit_model()->has_focus()) {
    return;
  }

  DeactivatePopupAndKillFocus(/*window_deactivated=*/true);

  // Set the browser window's stored focus view, which the `FocusManager`
  // restores whenever the window is reactivated. Do this after deactivating,
  // so that nothing run during it (closing the popup, `Hide()`) clears it, and
  // after activation has settled, so that the `FocusManager`'s own
  // `StoreFocusedView()` doesn't overwrite it.
  views::FocusManager* focus_manager = GetBrowserFocusManager();
  if (!focus_manager) {
    return;
  }
#if BUILDFLAG(IS_MAC)
  // On macOS, reactivating a window restores its previous focus, so restore
  // the location bar, which hands focus back to the popup via
  // `RequestFocus()`.
  focus_manager->SetStoredFocusView(
      delegate().GetLocationBarFocusRestoreView());
#else
  // TODO(b/567944511): Restore omnibox focus when the window is reactivated
  // without clicking into it (e.g. Alt+Tab) or by clicking top chrome, to
  // match the Views omnibox. On Aura, the stored focus view is restored before
  // the reactivating click is dispatched, so restoring the location bar here
  // would flash the popup open when the user clicks the page.
  if (auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
          GetBrowserWidget()->GetNativeWindow())) {
    focus_manager->SetStoredFocusView(browser_view->GetActiveContentsWebView());
  }
#endif  // BUILDFLAG(IS_MAC)
}

void OmniboxPopupFullPresenter::AppMenuClosed() {
  // The menu command runs before this is called, and may have activated
  // another window (e.g. "New window"). If so, blur as if the user left the
  // window. Don't refocus here, since `FocusManager::SetFocusedView()`
  // activates an inactive widget on non-Mac platforms and would steal
  // activation back.
  if (!IsBrowserWindowActive(GetBrowserWidget())) {
    BlurForWindowDeactivation();
    return;
  }
  if (IsShown() && controller()->popup_state_manager()->popup_state() ==
                       OmniboxPopupState::kFull) {
    if (auto* focus_manager = GetBrowserFocusManager()) {
      if (auto* restore_view = delegate().GetLocationBarFocusRestoreView()) {
        focus_manager->SetFocusedView(restore_view);
      }
    }
    RequestFocus();
    if (auto* popup_view = location_bar()->GetOmniboxPopupView()) {
      popup_view->OnFocus(/*query_zps=*/false);
    }
  }
}

void OmniboxPopupFullPresenter::FocusPopupContent() {
  if (auto* content = GetWebUIContent()) {
    content->RequestFocus();
    if (content->GetWebContents()) {
      content->GetWebContents()->Focus();
    }
  }
}

void OmniboxPopupFullPresenter::DeactivatePopupAndKillFocus(
    bool window_deactivated) {
  pending_focus_task_.Cancel();
  ResetPermissionPromptShowingState();
  is_deactivating_ = true;
  OmniboxEditModel* edit_model = controller()->edit_model();

  // If the view is showing text that's not user-text, revert the text to the
  // permanent display text. This usually occurs if Steady State Elisions is on
  // and the user has unelided, but not edited the URL.
  // Also revert if the text has been edited but currently exactly matches
  // the permanent text. An example of this scenario is someone typing on the
  // new tab page and then deleting everything using backspace/delete.
  if ((!edit_model->user_input_in_progress() &&
       edit_model->user_text() != edit_model->GetPermanentDisplayText()) ||
      (edit_model->user_input_in_progress() &&
       (edit_model->user_text() == edit_model->GetPermanentDisplayText() ||
        edit_model->user_text() ==
            controller()->client()->GetFormattedFullURL()))) {
    edit_model->Revert();
  }

  if (auto* popup_view = location_bar()->GetOmniboxPopupView()) {
    popup_view->OnBlur();
  }

  if (auto* focus_manager = GetBrowserFocusManager()) {
    views::View* restore_view = delegate().GetLocationBarFocusRestoreView();
    views::View* stored_view = focus_manager->GetStoredFocusView();
    if (stored_view == restore_view) {
      focus_manager->SetStoredFocusView(nullptr);
    }
    if (window_deactivated) {
      // The user left the browser window. Only clear Views focus here as
      // `ClearFocus()` also clears native focus, which on Windows calls
      // `::SetFocus()` on the browser HWND and reactivates it, stealing
      // activation back from the window the user switched to.
      focus_manager->SetFocusedView(nullptr);
    } else {
      focus_manager->ClearFocus();
    }
  }

  if (!window_deactivated) {
    controller()->client()->FocusWebContents();
  }
  edit_model->OnKillFocus();

  // Close the popup unless the user has an uncommitted draft.
  // NOTE: Query values directly from edit model, as they are not guaranteed to
  // remain consistent after calling `Revert()` or `OnKillFocus()`.
  if (!edit_model->user_input_in_progress() ||
      edit_model->user_text().empty()) {
    if (controller()->popup_state_manager()->popup_state() ==
        OmniboxPopupState::kFull) {
      controller()->popup_state_manager()->SetPopupState(
          OmniboxPopupState::kNone);
    }
  }

  is_deactivating_ = false;
}

void OmniboxPopupFullPresenter::WidgetDestroyed() {
  event_monitor_.reset();
  forward_events_timer_.Stop();
  popup_widget_observation_.Reset();
  // Update the popup state manager if widget was destroyed externally, e.g., by
  // the OS. This ensures the popup state manager stays in sync. Skip this when
  // `Hide()` is discarding the widget on purpose, since clearing the state here
  // would re-enter `Hide()`.
  if (!is_destroying_widget() &&
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kFull) {
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

void OmniboxPopupFullPresenter::StopForwardingEvents() {
  if (GetWidget()) {
    if (auto* results_frame =
            views::AsViewClass<FullWebUIOmniboxFrame>(GetResultsFrame())) {
      results_frame->SetForwardMouseEvents(false);
    }
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

  views::Widget* browser_widget = GetBrowserWidget();
  if (!browser_widget) {
    return;
  }

  // If neither this window nor its popup is currently active, ignore the click.
  if (!browser_widget->IsActive() &&
      !(GetWidget() && GetWidget()->IsActive())) {
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      browser_widget->GetNativeWindow());
  if (!browser_view) {
    return;
  }

  gfx::Point cursor_point = display::Screen::Get()->GetCursorScreenPoint();
  bool contains_top_container = false;

  if (browser_view->top_container()) {
    gfx::Rect top_container_bounds =
        browser_view->top_container()->GetBoundsInScreen();
    gfx::Rect window_bounds = browser_widget->GetWindowBoundsInScreen();
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

  if (contains_top_container) {
    // Clear autocomplete matches and reset `activeQueryId_` on WebUI only if
    // click is outside of the popup and in the top container while popup is
    // shown.
    if (IsShown()) {
      if (auto* content = GetWebUIContent()) {
        if (auto* popup_handler = content->popup_handler()) {
          popup_handler->ClearAutocompleteMatches();
        }
      }
    }
    return;
  }

  DeactivatePopupAndKillFocus(/*window_deactivated=*/false);
}

OmniboxFullPopupWebUIContent* OmniboxPopupFullPresenter::GetWebUIContent() {
  return static_cast<OmniboxFullPopupWebUIContent*>(
      OmniboxPopupPresenterBase::GetWebUIContent());
}

views::Widget* OmniboxPopupFullPresenter::GetBrowserWidget() {
  views::Widget* widget = delegate().GetLocationBarWidget();
  return widget ? widget->GetTopLevelWidget() : nullptr;
}

views::FocusManager* OmniboxPopupFullPresenter::GetBrowserFocusManager() {
  views::Widget* browser_widget = GetBrowserWidget();
  return browser_widget ? browser_widget->GetFocusManager() : nullptr;
}
