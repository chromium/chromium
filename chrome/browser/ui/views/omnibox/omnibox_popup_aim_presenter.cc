// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"

#include <optional>
#include <string_view>

#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/ax_mode.h"

OmniboxPopupAimPresenter::OmniboxPopupAimPresenter(
    LocationBarView* location_bar_view,
    OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar_view,
                                *location_bar_view,
                                controller) {
  SetWebUIContent(std::make_unique<OmniboxAimPopupWebUIContent>(
      this, location_bar_view, controller));
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
  // This method is called when the focus is transferred to or from this widget.
  // If a user clicks outside the popup, we will hide the popup.
  //
  // Separately, if a user opens a context menu inside this popup. The context
  // menu is a child widget so this popup widget is still considered active. We
  // will not hide the popup.
  if (!active &&
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim &&
      !location_bar()->in_popup_state_transition()) {
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
