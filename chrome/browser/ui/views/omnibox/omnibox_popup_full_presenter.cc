// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"

#include <memory>

#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/permissions/permission_request_manager.h"
#include "ui/views/focus/focus_manager.h"

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
  OmniboxPopupPresenterBase::Show();
  if (GetWidget() && !widget_observation_.IsObserving()) {
    widget_observation_.Observe(GetWidget());
  }
}

void OmniboxPopupFullPresenter::Hide() {
  widget_observation_.Reset();
  OmniboxPopupPresenterBase::Hide();
}

std::string_view OmniboxPopupFullPresenter::GetPopupMetricPrefix() const {
  return OmniboxPopupPresenterBase::kFullWebUIPopupMetricPrefix;
}

void OmniboxPopupFullPresenter::WidgetDestroyed() {
  // Update the popup state manager if widget was destroyed externally, e.g., by
  // the OS. This ensures the popup state manager stays in sync.
  if (controller()->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kFull) {
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
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

void OmniboxPopupFullPresenter::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  if (!active &&
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kFull &&
      !location_bar()->in_popup_state_transition()) {
    // Don't close popup if there's an active permission prompt.
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
