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
#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_utils.h"

OmniboxPopupFullPresenter::OmniboxPopupFullPresenter(
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate,
    OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar, presenter_delegate, controller) {
  SetWebUIContent(std::make_unique<OmniboxFullPopupWebUIContent>(
      this, this->location_bar(), controller));
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

  if (GetWidget() && !widget_observation_.IsObserving()) {
    widget_observation_.Observe(GetWidget());
  }
}

void OmniboxPopupFullPresenter::Hide() {
  forward_events_timer_.Stop();
  widget_observation_.Reset();
  OmniboxPopupPresenterBase::Hide();
}

void OmniboxPopupFullPresenter::RequestFocus() {
  if (GetWidget() && ShouldReceiveFocus()) {
    if (!GetWidget()->IsActive()) {
      GetWidget()->Activate();
      if (GetUIContainer() && GetUIContainer()->GetWidget()) {
        if (auto* focus_manager =
                GetUIContainer()->GetWidget()->GetFocusManager()) {
          // Clear stored focus on the container widget so that activating the
          // popup widget does not restore stale focus or steal focus back from
          // the WebUI input.
          focus_manager->SetStoredFocusView(nullptr);
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
          omnibox::kOmniboxAimDeferShowUntilVisualStateReady)) {
    return std::nullopt;
  }
  return base::Milliseconds(
      omnibox::kOmniboxAimDeferShowUntilVisualStateReadyTimeoutMs.Get());
}

bool OmniboxPopupFullPresenter::ShouldDetachWebContentsOnHide() const {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxAimDetachWebContentsOnHide);
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
  // If omnibox has received focus, it has been told by permission prompt that
  // permission prompt is closed and omnibox can behave normally again.
  // Therefore, turn off the 'embedded-permission-showing' flag that forces
  // omnibox to ignore focus-out events via function.
  if (active) {
    weak_factory_.InvalidateWeakPtrs();
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

  // Close full omnibox popup if there is no directive (like permission prompt
  // open) to ignore focus out events, and this is a focus out event.
  if (!active &&
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kFull &&
      !location_bar()->in_popup_state_transition() &&
      !IsPermissionPromptPreventingClose()) {
    // TODO(b/519724566): Look into using popup_closer here.
    // Clear results here since `DeactivatePopupAndKillFocus` can happen
    // after the new tab is opened and can pass on the stale results.
    controller()->StopAutocomplete(/*clear_result=*/true);

    // Delay killing focus and deactivating popup so that the tab state can
    // be saved before this operation.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<OmniboxPopupFullPresenter> presenter) {
                         if (presenter) {
                           presenter->DeactivatePopupAndKillFocus();
                         }
                       },
                       weak_factory_.GetWeakPtr()));
  }
}

void OmniboxPopupFullPresenter::DeactivatePopupAndKillFocus() {
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

  controller()->client()->FocusWebContents();
  controller()->edit_model()->OnKillFocus();

  if (GetWebUIContent() && GetWebUIContent()->contents_wrapper()) {
    if (auto* webui_controller =
            GetWebUIContent()->contents_wrapper()->GetWebUIController()) {
      if (auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller)) {
        if (auto* popup_handler = popup_ui->popup_handler()) {
          popup_handler->SetFocus(false);
        }
      }
    }
  }

  // If the user is currently typing do not close the popup.
  if (!user_input_in_progress || user_text.empty()) {
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

void OmniboxPopupFullPresenter::WidgetDestroyed() {
  forward_events_timer_.Stop();
  widget_observation_.Reset();
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
