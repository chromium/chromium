// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"

#include <optional>
#include <string_view>

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
  if (GetWebUIContent()) {
    auto* permission_manager =
        permissions::PermissionRequestManager::FromWebContents(
            GetWebUIContent()->GetWebContents());
    if (permission_manager && !permission_observation_.IsObserving()) {
      permission_observation_.Observe(permission_manager);
    }
  }
}

void OmniboxPopupAimPresenter::Hide() {
  widget_observation_.Reset();
  permission_observation_.Reset();
  is_handling_prompt_dismissal_ = false;
  ResetFocusRestorationState();
  OmniboxPopupPresenterBase::Hide();
}

std::string_view OmniboxPopupAimPresenter::GetPopupMetricPrefix() const {
  return OmniboxPopupPresenterBase::kAimPopupMetricPrefix;
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
  // Reset prompt dismissal tracking flag once focus/activation returns to the
  // omnibox.
  if (active) {
    is_handling_prompt_dismissal_ = false;
    ResetFocusRestorationState();
    return;
  }

  // This method is called when the focus is transferred to or from this widget.
  // If a user clicks outside the popup, we will hide the popup.
  //
  // Separately, if a user opens a context menu inside this popup. The context
  // menu is a child widget so this popup widget is still considered active. We
  // will not hide the popup.
  // If a permission prompt was just closed, ignore any out of focus events from
  // that.
  if (!active &&
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim &&
      !location_bar()->in_popup_state_transition() &&
      !is_handling_prompt_dismissal_) {
    if (base::FeatureList::IsEnabled(
            omnibox::kOmniboxKeepOpenOnFileSelection) &&
        is_restoring_focus_after_file_selection_) {
      // Focus restoration was interrupted or completed. Stop observing
      // FocusManager and clear the flag.
      ResetFocusRestorationState();
      return;
    }

    if (base::FeatureList::IsEnabled(
            omnibox::kOmniboxKeepOpenOnFileSelection) &&
        has_active_blockers()) {
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

void OmniboxPopupAimPresenter::OnPromptRemoved() {
  // When a permission prompt is removed (e.g., accepted or dismissed), focus
  // moves to the main web contents temporarily before returning to the omnibox
  // popup. Set `is_handling_prompt_dismissal_` to true and explicitly request
  // focus back to the omnibox (again, even though embedded prompt does so).
  // There is a task queue delay for this to be processed (or other focus events
  // before this), so the flag protects the omnibox from incorrectly closing due
  // to out of focus events until the omnibox gains focus (as requested here).
  is_handling_prompt_dismissal_ = true;
  if (location_bar()) {
    location_bar()->FocusLocation(/*is_user_initiated=*/false,
                                  /*clear_focus_if_failed=*/false);
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
  auto* location_bar_view = static_cast<LocationBarView*>(location_bar());
  if (is_restoring_focus_after_file_selection_ && location_bar_view) {
    // If the FocusManager is attempting to restore focus back to the omnibox
    // text field (which is the default behavior when the file picker closes and
    // the window re-activates), we intercept it and redirect focus to the
    // popup.
    if (focused_now == location_bar_view->omnibox_view()) {
      auto* fm = location_bar_view->GetWidget()->GetFocusManager();
      if (fm && fm->focus_change_reason() ==
                    views::FocusManager::FocusChangeReason::kFocusRestore) {
        RequestFocus();
        // Focus restored successfully. Clean up observation and flag.
        ResetFocusRestorationState();
      }
    } else if (focused_now) {
      // Focus went somewhere else. Cancel the restoration state.
      ResetFocusRestorationState();
    }
  }
}

void OmniboxPopupAimPresenter::ResetFocusRestorationState() {
  // Clear the restoration state and stop observing focus transitions.
  is_restoring_focus_after_file_selection_ = false;
  focus_manager_observation_.Reset();
}
