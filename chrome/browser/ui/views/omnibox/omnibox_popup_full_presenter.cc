// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/ash_element_identifiers.h"
#endif

OmniboxPopupFullPresenter::OmniboxPopupFullPresenter(
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate,
    OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar,
                                presenter_delegate,
                                controller,
                                OmniboxPopupContentConfig::ForFull()) {
  // `location_bar` may be null in unit tests, which run without content.
  if (location_bar) {
    ApplyWebUIContentCreationStrategy(WebUIContentCreationStrategy::kEager);
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
  if (auto* handler = controller ? controller->omnibox_handler() : nullptr) {
    // Compute visibility from the omnibox state rather than reading the Views
    // AIM chip. During a tab change, `Show()` runs before the location bar's
    // page action container is rebound to the new tab, so the chip still
    // reflects the deactivated previous tab and would report hidden, briefly
    // hiding the AIM button on new tab creation.
    handler->SetAimButtonVisible(
        omnibox::AiModePageActionController::ShouldShowPageAction(
            location_bar()->GetProfile(), *location_bar()));
  }

  views::Widget* browser_widget = GetBrowserWidget();
  if (browser_widget && !browser_widget_observation_.IsObserving()) {
    browser_widget_observation_.Observe(browser_widget);
  }
  if (views::FocusManager* focus_manager = GetBrowserFocusManager();
      focus_manager && !browser_focus_manager_observation_.IsObserving()) {
    browser_focus_manager_observation_.Observe(focus_manager);
  }
  if (GetWidget() && !popup_widget_observation_.IsObserving()) {
    popup_widget_observation_.Observe(GetWidget());
  }

  if (browser_widget && !event_monitor_) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, browser_widget->GetNativeWindow(),
        {ui::EventType::kMousePressed, ui::EventType::kTouchPressed});
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
#if BUILDFLAG(IS_CHROMEOS)
      if (browser_view->top_container() &&
          !top_container_observation_.IsObserving()) {
        top_container_observation_.Observe(browser_view->top_container());
      }
#endif
      if (auto* elements =
              BrowserElementsViews::From(browser_view->browser())) {
        for (views::View* view :
             {elements->RetrieveView(kVerticalTabStripRegionRetrievalId),
              elements->GetView(OrganizerTrayView::kTrayElementId,
                                /*require_visible=*/false)}) {
          if (view && !occluding_view_observations_.IsObservingSource(view)) {
            occluding_view_observations_.AddObservation(view);
          }
        }
      }
      UpdatePopupOcclusion();
    }
  }
}

void OmniboxPopupFullPresenter::Hide() {
  pending_focus_task_.Cancel();
  refocus_on_activation_ = false;
  // The popup stops being observed before it's hidden.
  KeepBrowserPaintingAsActiveAcrossHandoff();
  browser_widget_observation_.Reset();
  browser_focus_manager_observation_.Reset();
  // Cancel any pending context menu request, since the base `Hide()` only
  // clears the content (which also cancels it) when it closes the widget. A
  // request left pending could show the menu on a later activation.
  if (auto* content = GetWebUIContent()) {
    content->CancelPendingContextMenuRequest();
  }
  app_menu_control_observation_.Reset();
  occluding_view_observations_.RemoveAllObservations();
#if BUILDFLAG(IS_CHROMEOS)
  top_container_observation_.Reset();
#endif
  UpdatePopupOcclusion();

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
  if (config().height_workarounds) {
    // Reset the cached height to force a layout update when the popup is
    // reshown. This prevents the popup from temporarily using a stale size
    // from its previous state.
    content_height_ = 1;
  }
}

void OmniboxPopupFullPresenter::OnOmniboxFocused() {
  OmniboxPopupPresenterBase::OnOmniboxFocused();

  // The popup is its own widget, so its focus never reaches the tray's focus
  // listener. The panel embedded in the vertical tab strip has no such
  // listener and stays open, as it does for the Views omnibox.
  if (views::Widget* browser_widget = GetBrowserWidget()) {
    if (auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
            browser_widget->GetNativeWindow())) {
      if (auto* controller =
              OrganizerPanelController::From(browser_view->browser());
          controller && controller->GetCurrentOrganizerPanelLocation() ==
                            OrganizerPanelLocation::kOrganizerTray) {
        controller->SetOrganizerVisible(false);
      }
    }
  }
}

void OmniboxPopupFullPresenter::RequestFocus() {
  if (views::MenuController::GetActiveInstance()) {
    return;
  }
  if (ShouldPreserveRequestedFocus()) {
    focus_requested_ = true;
  }

  if (!GetWidget() || !ShouldReceiveFocus()) {
    return;
  }

  // Don't take focus in a browser window the user has left. Activating the
  // popup would pull the window back in front of the one the user switched to.
  if (!CanTakeFocus()) {
    // The omnibox stays focused, e.g. on the NTP of a new window that a
    // window-modal dialog took activation from before the omnibox was focused,
    // as no deactivation blurred it. Hand focus to the popup once the window is
    // activated again, as the Views omnibox's `FocusManager` does, so that its
    // caret shows and key presses reach it. See `OnWidgetActivationChanged()`.
    refocus_on_activation_ = true;
    return;
  }
  refocus_on_activation_ = false;

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
  // A request made while the browser window is active must not take
  // activation from a bubble that opens before the task runs.
  const bool was_browser_or_popup_active = IsBrowserOrPopupActive();
  pending_focus_task_.Reset(base::BindOnce(
      [](base::WeakPtr<OmniboxPopupFullPresenter> presenter,
         bool was_browser_or_popup_active) {
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
        // queued (e.g. a bubble opened). Only a request made while a bubble
        // already held activation may take activation from it.
        if (was_browser_or_popup_active ? !presenter->IsBrowserOrPopupActive()
                                        : !presenter->CanTakeFocus()) {
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
      weak_factory_.GetWeakPtr(), was_browser_or_popup_active));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, pending_focus_task_.callback());
}

std::string_view OmniboxPopupFullPresenter::GetPopupMetricPrefix() const {
  return OmniboxPopupPresenterBase::kFullWebUIPopupMetricPrefix;
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

bool OmniboxPopupFullPresenter::ShouldReceiveFocus() const {
  if (views::MenuController::GetActiveInstance()) {
    return false;
  }
  return OmniboxPopupPresenterBase::ShouldReceiveFocus();
}

gfx::Insets OmniboxPopupFullPresenter::GetShadowMargin() const {
  gfx::Insets margin = RoundedOmniboxResultsFrame::GetShadowInsets();
  // While collapsed nothing is painted below the location bar, so drop the
  // bottom margin to keep the widget from covering the UI directly beneath it
  // (e.g. the bookmarks bar or the top of the web contents).
  if (IsPopupCollapsed()) {
    margin.set_bottom(0);
  }
  return margin;
}

bool OmniboxPopupFullPresenter::IsPopupCollapsed() const {
  const int location_bar_height =
      location_bar() ? location_bar()->BoundsInScreen().height()
                     : GetLayoutConstant(LayoutConstant::kLocationBarHeight);
  return content_height_ <=
         location_bar_height +
             FullWebUIOmniboxFrame::GetLocationBarAlignmentInsets().height();
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
  const gfx::Insets shadow_margin = GetShadowMargin();
  results_frame->SetShadowMargin(shadow_margin);
  const bool shadow_in_webui = config().draw_shadow_in_webui;
  if (!shadow_in_webui) {
    const bool has_results = content_height_ > default_height;
    results_frame->SetElevation(
        has_results ? RoundedOmniboxResultsFrame::kDefaultElevation : 0);
  }

  // Never size the popup shorter than the location bar. While collapsed the
  // WebView is clipped by the dropped bottom shadow margin, so a height seeded
  // from its view bounds (see `OmniboxPopupWebUIBaseContent::Show()`) can
  // under-report by that margin.
  widget_bounds.set_height(std::max(content_height_, default_height));

  // Set width and height to at least their minimums (e.g. for permission
  // prompts).
  widget_bounds.set_width(
      std::max(get_minimum_size().width(), widget_bounds.width()));
  widget_bounds.set_height(
      std::max(get_minimum_size().height(), widget_bounds.height()));

  // Normally the frame's border supplies the shadow margin. When the page
  // paints the shadow the frame has no border, so add the margin here; the
  // WebView covers it and the page paints the shadow into it. While collapsed
  // the bottom margin is dropped, which clips the page's (transparent) bottom
  // shadow padding.
  widget_bounds.Inset(
      -(shadow_in_webui ? shadow_margin : results_frame->GetInsets()));
  GetWidget()->SetBounds(widget_bounds);

  UpdatePopupOcclusion();
}

void OmniboxPopupFullPresenter::NotifyEscapeKeyPressed() {
  is_handling_escape_key_ = true;
}

void OmniboxPopupFullPresenter::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  if (active && refocus_on_activation_) {
    // A focus request couldn't take activation (see `RequestFocus()`), and the
    // browser window or the popup is now active, e.g. after the window-modal
    // dialog that held activation closed. Depending on the window manager,
    // either may be activated, and neither focuses the WebUI content by
    // itself. Refocus once activation settles.
    refocus_on_activation_ = false;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&OmniboxPopupFullPresenter::RefocusAfterActivation,
                       weak_factory_.GetWeakPtr()));
  }

  // In immersive fullscreen on macOS, the popup's parent is an overlay, so
  // activating the popup doesn't lock the browser window.
  if (active && widget == GetWidget() && !browser_paint_as_active_lock_) {
    if (views::Widget* browser_widget = GetBrowserWidget()) {
      browser_paint_as_active_lock_ = browser_widget->LockPaintAsActive();
    }
  }

  if (!active) {
    KeepBrowserPaintingAsActiveAcrossHandoff();

    // The browser or popup widget deactivating may mean another window, such
    // as a bubble, took activation. Check once activation settles, since
    // handoffs between the browser and the popup deactivate one widget before
    // activating the next. See `IsBrowserOrPopupActive()`.
    //
    // Must be posted before the stored focus view task below, so that the
    // omnibox is blurred by the time that task runs and it does nothing.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&OmniboxPopupFullPresenter::BlurIfBrowserOrPopupInactive,
                       weak_factory_.GetWeakPtr()));
  }

  // If the widget that changed is the browser window.
  if (widget == GetBrowserWidget()) {
    if (!active) {
      // When the browser window deactivates (e.g. due to focusing the WebUI
      // popup), we must cache the Omnibox view as the stored focus view. This
      // ensures that when the browser window reactivates, the FocusManager
      // restores focus to the Omnibox.
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
      // as returning to the omnibox. Skip when a mouse button is down: AppKit
      // makes the popup window key before dispatching `mouseDown:`, so focusing
      // here would reveal UI (such as the AIM button) under the cursor before
      // Blink hit-tests the press; the click itself will focus the input. On
      // Aura, the popup can become active when the browser window deactivates,
      // so its activation isn't a reliable signal.
      if (IsShown() && !controller()->edit_model()->has_focus() &&
          !widget->IsMouseButtonDown()) {
        if (auto* popup_view = location_bar()->GetOmniboxPopupView()) {
          popup_view->OnFocus(/*query_zps=*/false, /*select_all=*/false,
                              /*show_full_url=*/false);
        }
      }
#endif  // BUILDFLAG(IS_MAC)
      if (auto* content = GetWebUIContent()) {
        content->OnPopupWidgetActivationChanged(/*active=*/true);
      }
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

    if (auto* content = GetWebUIContent()) {
      content->OnPopupWidgetActivationChanged(/*active=*/false);
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

void OmniboxPopupFullPresenter::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  // Only the browser widget. The popup widget is observed too, and
  // `SynchronizePopupBounds()` changes its bounds.
  if (widget != GetBrowserWidget() || !IsShown()) {
    return;
  }
  SynchronizePopupBounds();
}

void OmniboxPopupFullPresenter::RefocusAfterActivation() {
  // Only refocus if the omnibox is still focused with the popup open. If the
  // window was deactivated again in the meantime, `RequestFocus()` waits for
  // the next activation.
  if (!IsShown() ||
      controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kFull ||
      !controller()->edit_model()->has_focus()) {
    return;
  }
  RequestFocus();
}

void OmniboxPopupFullPresenter::BlurIfBrowserOrPopupInactive() {
  if (IsBrowserOrPopupActive()) {
    return;
  }
#if BUILDFLAG(IS_CHROMEOS)
  // The system emoji picker took activation, e.g. from the Emoji item of the
  // omnibox's context menu. Once an emoji is picked, it closes and inserts the
  // emoji into the text input that is focused when its window is reactivated,
  // as the Views omnibox's `FocusManager` restores focus to it. Keep the
  // omnibox focused with the popup open, hand focus back to the popup once
  // the browser window or popup is reactivated, and check again once the
  // picker closes, in case the user left the browser window instead.
  if (IsEmojiPickerShown() &&
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kFull &&
      controller()->edit_model()->has_focus()) {
    refocus_on_activation_ = true;
    if (!emoji_picker_hidden_subscription_) {
      emoji_picker_hidden_subscription_ =
          ui::ElementTracker::GetElementTracker()
              ->AddElementHiddenInAnyContextCallback(
                  ash::kEmojiPickerElementId,
                  base::BindRepeating(
                      &OmniboxPopupFullPresenter::OnEmojiPickerHidden,
                      base::Unretained(this)));
    }
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  BlurForWindowDeactivation();
}

#if BUILDFLAG(IS_CHROMEOS)
// static
bool OmniboxPopupFullPresenter::IsEmojiPickerShown() {
  return !!ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      ash::kEmojiPickerElementId);
}

void OmniboxPopupFullPresenter::OnEmojiPickerHidden(
    ui::TrackedElement* element) {
  emoji_picker_hidden_subscription_ = {};
  // Check once activation has moved to the next window.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OmniboxPopupFullPresenter::BlurIfBrowserOrPopupInactive,
                     weak_factory_.GetWeakPtr()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void OmniboxPopupFullPresenter::BlurForWindowDeactivation() {
  if (controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kFull ||
      !controller()->edit_model()->has_focus()) {
    return;
  }

  DeactivatePopupAndKillFocus(FocusAfterBlur::kNone);

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
  delegate().SetRestoreFocusOnWindowReactivation(true);
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
  if (!IsBrowserOrPopupActive()) {
    BlurForWindowDeactivation();
    return;
  }
  // Only refocus if the omnibox still has focus. The command may have moved
  // focus to another view in the window (e.g. "Find" focuses the find bar),
  // which blurred the omnibox, while a draft kept the popup open.
  if (IsShown() &&
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kFull &&
      controller()->edit_model()->has_focus()) {
    if (auto* focus_manager = GetBrowserFocusManager()) {
      if (auto* restore_view = delegate().GetLocationBarFocusRestoreView()) {
        focus_manager->SetFocusedView(restore_view);
      }
    }
    RequestFocus();
    if (auto* popup_view = location_bar()->GetOmniboxPopupView()) {
      popup_view->OnFocus(/*query_zps=*/false, /*select_all=*/false,
                          /*show_full_url=*/false);
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

void OmniboxPopupFullPresenter::OnDidChangeFocus(views::View* focused_before,
                                                 views::View* focused_now) {
  // Focus being cleared is the browser window deactivating or the omnibox
  // being blurred, not another view taking focus.
  if (!focused_now || !controller()->edit_model()->has_focus()) {
    return;
  }

  // Focusing the location bar is how the omnibox itself gets focus, see
  // `LocationBarView::OnFocus()`. Focus moving within it is left alone too.
  views::View* restore_view = delegate().GetLocationBarFocusRestoreView();
  if (restore_view && restore_view->Contains(focused_now)) {
    return;
  }

  // Focus moving to the page is left alone. Clicks on the page are handled by
  // `OnEvent()`, and `BrowserView` can focus the page while switching tabs,
  // before `OmniboxPopupViewFullWebUI::OnTabChanged()` applies the new tab's
  // omnibox state, which must not blur the omnibox.
  if (views::IsViewClass<ContentsWebView>(focused_now)) {
    return;
  }

  // The popup holds activation, so the browser widget is inactive and
  // focusing `focused_now` usually activated it first (see
  // `FocusManager::SetFocusedViewWithReason()`). This runs while that
  // activation is handled, and `BrowserView::OnWidgetActivationChanged()`
  // runs right after. If a tab was switched while the widget was inactive,
  // `BrowserView::OnActiveTabChanged()` deferred restoring the new tab's
  // focus to this activation, so `BrowserView` is about to focus the view that
  // tab last stored (e.g. the omnibox), taking focus back from `focused_now`.
  // Store `focused_now` as the tab's focus so that the restore is a no-op.
  if (content::WebContents* web_contents = location_bar()->GetWebContents()) {
    if (auto* focus_helper =
            ChromeWebContentsViewFocusHelper::FromWebContents(web_contents)) {
      focus_helper->SetStoredFocusView(focused_now);
    }
  }

  // Tabbing out of the popup also gets here, as
  // `OmniboxPopupWebUIBaseContent::AdvanceFocus()` moves focus to the next
  // view before running its own blur steps, which are safe to repeat.
  DeactivatePopupAndKillFocus(FocusAfterBlur::kCurrent);

  // Key presses go to the active window. On macOS the popup is the key
  // window while the omnibox is focused, and stays so if a draft keeps it
  // open. Focusing a view in an inactive window usually activates the window
  // first, as on Aura, and then this does nothing. When macOS focuses the
  // view without activating (see
  // `NativeWidgetMac::ShouldActivateOnFocusRequest()`), activate the browser
  // window for the view that took focus. Its `FocusManager` already has that
  // view as its focused view, so activating restores nothing else. Skip this
  // if the user has left the browser window, as activating it would pull it
  // in front of the window they switched to.
  if (views::Widget* browser_widget = GetBrowserWidget();
      browser_widget && !browser_widget->IsActive() &&
      IsBrowserOrPopupActive()) {
    browser_widget->Activate();
  }
}

void OmniboxPopupFullPresenter::DeactivatePopupAndKillFocus(
    FocusAfterBlur focus_after_blur) {
  pending_focus_task_.Cancel();
  refocus_on_activation_ = false;
  ResetPermissionPromptShowingState();
  OmniboxEditModel* edit_model = controller()->edit_model();

  // Keyword mode is an uncommitted draft even when the user text is empty,
  // e.g. on the NTP after entering keyword mode via the keyword chip. Blurring
  // should never exit keyword mode (see `OmniboxViewViews::OnBlur()`), so
  // neither revert nor close the popup in that case.
  const bool is_keyword_selected = edit_model->is_keyword_selected();

  // If the view is showing text that's not user-text, revert the text to the
  // permanent display text. This usually occurs if Steady State Elisions is on
  // and the user has unelided, but not edited the URL.
  // Also revert if the text has been edited but currently exactly matches
  // the permanent text. An example of this scenario is someone typing on the
  // new tab page and then deleting everything using backspace/delete.
  //
  // This should never exit keyword mode.
  if (!is_keyword_selected &&
      ((!edit_model->user_input_in_progress() &&
        edit_model->user_text() != edit_model->GetPermanentDisplayText()) ||
       (edit_model->user_input_in_progress() &&
        (edit_model->user_text() == edit_model->GetPermanentDisplayText() ||
         edit_model->user_text() ==
             controller()->client()->GetFormattedFullURL())))) {
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
    switch (focus_after_blur) {
      case FocusAfterBlur::kWebContents:
        focus_manager->ClearFocus();
        break;
      case FocusAfterBlur::kNone:
        // Another window, such as a bubble, took activation. Only clear Views
        // focus here as `ClearFocus()` also clears native focus, which on
        // Windows calls `::SetFocus()` on the browser HWND and reactivates it,
        // stealing activation back from that window.
        focus_manager->SetFocusedView(nullptr);
        break;
      case FocusAfterBlur::kCurrent:
        // Another view took focus. Leave it there.
        break;
    }
  }

  if (focus_after_blur == FocusAfterBlur::kWebContents) {
    controller()->client()->FocusWebContents();
  }
  edit_model->OnKillFocus();

  // Close the popup unless the user has an uncommitted draft. Keyword mode
  // counts as a draft even if the user text is empty.
  // NOTE: Query values directly from edit model, as they are not guaranteed to
  // remain consistent after calling `Revert()` or `OnKillFocus()`.
  if (!edit_model->HasDraft()) {
    if (controller()->popup_state_manager()->popup_state() ==
        OmniboxPopupState::kFull) {
      controller()->popup_state_manager()->SetPopupState(
          OmniboxPopupState::kNone);
    }
  }
}

void OmniboxPopupFullPresenter::OnWidgetPresentedChanged(bool presented) {
  if (auto* content = GetWebUIContent()) {
    content->OnPopupWidgetPresentedChanged(presented);
  }
}

std::unique_ptr<OmniboxPopupWebUIBaseContent>
OmniboxPopupFullPresenter::CreateWebUIContent(
    std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents) {
  return std::make_unique<OmniboxFullPopupWebUIContent>(
      /*content_delegate=*/this, /*sizer=*/this, /*config=*/config(),
      location_bar(), controller(), std::move(contents));
}

void OmniboxPopupFullPresenter::WidgetDestroyed() {
  event_monitor_.reset();
  forward_events_timer_.Stop();
  popup_widget_observation_.Reset();
  KeepBrowserPaintingAsActiveAcrossHandoff();
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
  if (event.type() != ui::EventType::kMousePressed &&
      event.type() != ui::EventType::kTouchPressed) {
    return;
  }

  // Right-clicks (context menu triggers) should never clear focus.
  if (event.IsMouseEvent() && event.AsMouseEvent()->IsRightMouseButton()) {
    return;
  }

  views::Widget* browser_widget = GetBrowserWidget();
  if (!browser_widget) {
    return;
  }

  // If neither this window nor its popup is currently active, ignore the click.
  if (!IsBrowserOrPopupActive()) {
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      browser_widget->GetNativeWindow());
  if (!browser_view) {
    return;
  }

  gfx::Point event_screen_point;
  if (event.IsTouchEvent()) {
    // The event monitor already reports `root_location()` in screen
    // coordinates.
    event_screen_point = event.AsTouchEvent()->root_location();
  } else {
    event_screen_point = display::Screen::Get()->GetCursorScreenPoint();
  }

  bool contains_top_container = false;

  if (browser_view->top_container()) {
    gfx::Rect top_container_bounds =
        browser_view->top_container()->GetBoundsInScreen();
    gfx::Rect window_bounds = browser_widget->GetWindowBoundsInScreen();
    contains_top_container =
        event_screen_point.x() >= window_bounds.x() &&
        event_screen_point.x() < window_bounds.right() &&
        event_screen_point.y() >= window_bounds.y() &&
        event_screen_point.y() < top_container_bounds.bottom();
  }

  // Treat vertical tabs and the organizer tray, outside the top container,
  // like horizontal tabs. Otherwise clicking a tab blurs the omnibox before
  // the tab switch saves its state (see crbug.com/580059964), and clicking
  // the tray focuses the web contents and closes the tray.
  for (views::View* view : occluding_view_observations_.sources()) {
    if (view->IsDrawn() &&
        view->GetBoundsInScreen().Contains(event_screen_point)) {
      contains_top_container = true;
      break;
    }
  }

  bool contains_popup = false;
  if (IsShown()) {
    // Exclude the transparent shadow margin and the part clipped off under the
    // vertical tab strip: on Aura the window targeter lets clicks there pass
    // through to the browser window, so they are outside.
    const gfx::Rect window_bounds = GetWidget()->GetWindowBoundsInScreen();
    gfx::Rect popup_bounds = window_bounds;
    popup_bounds.Inset(GetShadowMargin());
    if (const ui::Layer* layer = GetWidget()->GetLayer();
        layer && !layer->clip_rect().IsEmpty()) {
      popup_bounds.Intersect(layer->clip_rect() +
                             window_bounds.OffsetFromOrigin());
    }
    contains_popup = popup_bounds.Contains(event_screen_point);
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

  DeactivatePopupAndKillFocus(FocusAfterBlur::kWebContents);
}

OmniboxFullPopupWebUIContent* OmniboxPopupFullPresenter::GetWebUIContent() {
  return static_cast<OmniboxFullPopupWebUIContent*>(
      OmniboxPopupPresenterBase::GetWebUIContent());
}

bool OmniboxPopupFullPresenter::IsBrowserOrPopupActive() {
  views::Widget* browser_widget = GetBrowserWidget();
  if (!browser_widget) {
    return false;
  }
  return browser_widget->IsActive() ||
         (GetWidget() && GetWidget()->IsActive());
}

bool OmniboxPopupFullPresenter::CanTakeFocus() {
  if (IsBrowserOrPopupActive()) {
    return true;
  }
#if BUILDFLAG(IS_MAC)
  // A focus request can arrive while a bubble anchored to the browser window
  // still holds activation, e.g. one that doesn't move Views focus and so
  // doesn't activate the browser window first. The user hasn't left the
  // window, and paint-as-active stays true while such a child widget is active.
  views::Widget* browser_widget = GetBrowserWidget();
  return browser_widget && browser_widget->ShouldPaintAsActive();
#else
  return false;
#endif  // BUILDFLAG(IS_MAC)
}

views::Widget* OmniboxPopupFullPresenter::GetBrowserWidget() {
  views::Widget* widget = delegate().GetLocationBarWidget();
  return widget ? widget->GetTopLevelWidget() : nullptr;
}

views::FocusManager* OmniboxPopupFullPresenter::GetBrowserFocusManager() {
  views::Widget* browser_widget = GetBrowserWidget();
  return browser_widget ? browser_widget->GetFocusManager() : nullptr;
}

void OmniboxPopupFullPresenter::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view,
    bool visible) {
  UpdatePopupOcclusion();
}

void OmniboxPopupFullPresenter::OnViewBoundsChanged(
    views::View* observed_view) {
#if BUILDFLAG(IS_CHROMEOS)
  // Keep the popup over the location bar as the immersive fullscreen reveal
  // slides top chrome in or out.
  if (top_container_observation_.IsObservingSource(observed_view) &&
      IsShown()) {
    SynchronizePopupBounds();
  }
#endif
  UpdatePopupOcclusion();
}

void OmniboxPopupFullPresenter::OnViewIsDeleting(views::View* observed_view) {
#if BUILDFLAG(IS_CHROMEOS)
  if (top_container_observation_.IsObservingSource(observed_view)) {
    top_container_observation_.Reset();
  }
#endif
  if (occluding_view_observations_.IsObservingSource(observed_view)) {
    occluding_view_observations_.RemoveObservation(observed_view);
  }
  UpdatePopupOcclusion();
}

void OmniboxPopupFullPresenter::UpdatePopupOcclusion() {
  // `GetLayer()` is null if the native window is already closing.
  ui::Layer* layer = GetWidget() ? GetWidget()->GetLayer() : nullptr;
  if (!layer) {
    return;
  }
  gfx::Rect clip;
  // Only clip while the popup is collapsed. Once suggestions open, the strip
  // collapses and the popup should paint in full. The widget comparison skips
  // macOS immersive fullscreen, where the location bar sits in an overlay
  // widget above the strip and tray.
  if (IsPopupCollapsed()) {
    const gfx::Rect popup_bounds = GetWidget()->GetWindowBoundsInScreen();
    views::Widget* location_bar_widget = delegate().GetLocationBarWidget();
    gfx::Rect covered;
    for (views::View* view : occluding_view_observations_.sources()) {
      if (view->IsDrawn() && view->GetWidget() == location_bar_widget) {
        gfx::Rect intersection = view->GetBoundsInScreen();
        intersection.Intersect(popup_bounds);
        covered.Union(intersection);
      }
    }
    if (!covered.IsEmpty()) {
      // `clip` is the uncovered portion of the popup.
      clip = gfx::Rect(popup_bounds.size());
      clip.Inset(covered.x() == popup_bounds.x()
                     ? gfx::Insets::TLBR(0, covered.width(), 0, 0)
                     : gfx::Insets::TLBR(0, 0, 0, covered.width()));
      if (clip.IsEmpty()) {
        // Nothing is left uncovered (`covered.width() ==
        // popup_bounds.width()`). `SetClipRect()` treats an empty rect as "no
        // clip", so clip to a 1px rect outside the window instead.
        clip = gfx::Rect(-1, -1, 1, 1);
      }
    }
  }
  // Ensure events in the covered part reach the view beneath (via
  // `aura::WindowTargeter::GetHitTestRects()` on Aura and
  // `ResultsViewTargeterDelegate::TargetForRect()` on macOS).
  layer->SetClipRect(clip);
}

void OmniboxPopupFullPresenter::KeepBrowserPaintingAsActiveAcrossHandoff() {
  views::Widget* browser_widget = GetBrowserWidget();
  if (!browser_widget || !browser_widget->ShouldPaintAsActive()) {
    return;
  }
  if (!browser_paint_as_active_lock_) {
    browser_paint_as_active_lock_ = browser_widget->LockPaintAsActive();
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OmniboxPopupFullPresenter::
                         ReleaseBrowserPaintAsActiveLockIfPopupInactive,
                     weak_factory_.GetWeakPtr()));
}

void OmniboxPopupFullPresenter::
    ReleaseBrowserPaintAsActiveLockIfPopupInactive() {
  if (!GetWidget() || !GetWidget()->IsActive()) {
    browser_paint_as_active_lock_.reset();
  }
}
