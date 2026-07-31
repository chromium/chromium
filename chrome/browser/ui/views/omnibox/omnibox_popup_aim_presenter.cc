// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"

#include <optional>
#include <string_view>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/permissions/permission_request_manager.h"

OmniboxPopupAimPresenter::OmniboxPopupAimPresenter(
    LocationBar* location_bar,
    OmniboxController* controller,
    OmniboxPopupPresenterDelegate& presenter_delegate)
    : OmniboxPopupPresenterBase(location_bar, presenter_delegate, controller) {
  SetWebUIContent(std::make_unique<OmniboxAimPopupWebUIContent>(
      this, location_bar, controller));
}

OmniboxPopupAimPresenter::~OmniboxPopupAimPresenter() = default;

void OmniboxPopupAimPresenter::Show() {
  const bool was_shown = IsShown();
  OmniboxPopupPresenterBase::Show();
  if (!was_shown && IsShown()) {
    // Set the request time to now when the popup is first shown. This ensures
    // that latency is measured from the user interaction to show, even if the
    // WebUI was preloaded at startup.
    WebUIContentsPreloadManager::GetInstance()->SetRequestTime(
        GetWebUIContent()->GetWebContents(), base::TimeTicks::Now());
  }
  if (GetWidget() && !widget_observation_.IsObserving()) {
    widget_observation_.Observe(GetWidget());
  }
}

void OmniboxPopupAimPresenter::Hide() {
  widget_observation_.Reset();
  ResetFocusRestorationState();
  OmniboxPopupPresenterBase::Hide();
}

std::string_view OmniboxPopupAimPresenter::GetPopupMetricPrefix() const {
  return OmniboxPopupPresenterBase::kAimPopupMetricPrefix;
}

void OmniboxPopupAimPresenter::LogResultToContentReadyMetric(
    base::TimeTicks /*result_ready_time*/,
    bool /*is_first_show*/) {
  // The AIM popup calculates its ready state inherently differently than
  // standard dropdowns. We explicitly override this metric to a no-op here to
  // prevent logging garbage data and to avoid polluting standard WebUI
  // telemetry.
}

std::optional<base::TimeDelta>
OmniboxPopupAimPresenter::ShouldDeferUntilVisualStateReady() const {
  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxAimDeferShowUntilVisualStateReady)) {
    return std::nullopt;
  }
  return base::Milliseconds(
      omnibox::kOmniboxAimDeferShowUntilVisualStateReadyTimeoutMs.Get());
}

bool OmniboxPopupAimPresenter::ShouldDetachWebContentsOnHide() const {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxAimDetachWebContentsOnHide);
}

void OmniboxPopupAimPresenter::OnWidgetActivationChanged(views::Widget* widget,
                                                         bool active) {
  if (active) {
    // If omnibox has received focus, it has been told by permission prompt that
    // permission prompt is closed and omnibox can behave normally again.
    // Therefore, turn off the 'is_handling_prompt_dismissal_' flag that forces
    // omnibox to ignore focus-out events via function.
    OnWidgetActivated();
    return;
  }

  // This method is called when the focus is transferred to or from this widget.
  // If a user clicks outside the popup, we will hide the popup.
  //
  // Separately, if a user opens a context menu inside this popup. The context
  // menu is a child widget so this popup widget is still considered active. We
  // will not hide the popup.
  // If a permission prompt is showing or was just closed, ignore any out of
  // focus events from that.
  if (!active &&
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim &&
      !location_bar()->in_popup_state_transition() &&
      !IsPermissionPromptPreventingClose()) {
    // If keep open on file selection is enabled, don't close the popup if:
    // 1) An active deactivation blocker is held (while the OS file dialog is
    //    open).
    // 2) We are restoring focus after file selection (after the dialog
    //    closes and the blocker is released, during window reactivation focus
    //    transfer).
    if (base::FeatureList::IsEnabled(
            omnibox::kOmniboxKeepOpenOnFileSelection) &&
        (has_active_blockers() || is_restoring_focus_after_file_selection_)) {
      return;
    }

    // Don't close popup if there's an active permission prompt. This check can
    // be reached when the permission prompt has just been shown for Voice
    // permission from the omnibox popup and interacting with the prompt has
    // caused focus to leave the popup, causing it to close unexpectedly.
    if (auto* content = GetWebUIContent()) {
      auto* permission_manager =
          permissions::PermissionRequestManager::FromWebContents(
              content->GetWebContents());
      if (permission_manager && permission_manager->IsRequestInProgress()) {
        return;
      }
    }
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

void OmniboxPopupAimPresenter::WidgetDestroyed() {
  // Update the popup state manager if widget was destroyed externally, e.g., by
  // the OS. This ensures the popup state manager stays in sync.
  if (controller()->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kAim) {
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

void OmniboxPopupAimPresenter::OnFileSelectionClosed() {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxKeepOpenOnFileSelection)) {
    return;
  }
  // When a file picker is closed, the browser window will regain focus shortly.
  // We set `is_restoring_focus_after_file_selection_` to true and start
  // observing the FocusManager to intercept the upcoming focus restoration.
  is_restoring_focus_after_file_selection_ = true;
  auto* location_bar_view = static_cast<LocationBarView*>(location_bar());
  if (location_bar_view && location_bar_view->GetWidget()) {
    if (auto* fm = location_bar_view->GetWidget()->GetFocusManager()) {
      focus_manager_observation_.Observe(fm);
    }
  }
}

void OmniboxPopupAimPresenter::OnDidChangeFocus(views::View* focused_before,
                                                views::View* focused_now) {
  // Ignore calls before focus restoration, and focus clears during
  // transitions.
  if (!is_restoring_focus_after_file_selection_ || !focused_now) {
    return;
  }

  auto* location_bar_view = static_cast<LocationBarView*>(location_bar());
  // When the file picker closes and the window re-activates, `FocusManager`
  // attempts to restore focus back to the native omnibox text field. We
  // intercept this and redirect focus back to the WebUI popup.
  if (location_bar_view && focused_now == location_bar_view->omnibox_view()) {
    auto* fm = location_bar_view->GetWidget()->GetFocusManager();
    if (fm && fm->focus_change_reason() ==
                  views::FocusManager::FocusChangeReason::kFocusRestore) {
      // Defer `FinishFocusRestoration` asynchronously to prevent re-entrant
      // widget activation and focus shifts while `views::FocusManager` is in
      // the middle of restoring window focus.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&OmniboxPopupAimPresenter::FinishFocusRestoration,
                         weak_factory_.GetWeakPtr()));
      return;
    }
  }

  // Focus intentionally went to a completely different view (e.g., a toolbar
  // button). We should ensure the popup closes.
  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);

  // Reset state and stop observing focus transitions.
  ResetFocusRestorationState();
}

void OmniboxPopupAimPresenter::FinishFocusRestoration() {
  // Activate popup widget and focus WebContents.
  RequestFocus();
  // Focus the WebUI's text input element.
  if (auto* content =
          static_cast<OmniboxAimPopupWebUIContent*>(GetWebUIContent())) {
    content->FocusInput();
  }
  ResetFocusRestorationState();
}

void OmniboxPopupAimPresenter::ResetFocusRestorationState() {
  is_restoring_focus_after_file_selection_ = false;
  focus_manager_observation_.Reset();
}
