// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
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
  widget_observation_.Reset();
  OmniboxPopupPresenterBase::Hide();
}

std::string_view OmniboxPopupFullPresenter::GetPopupMetricPrefix() const {
  return OmniboxPopupPresenterBase::kFullWebUIPopupMetricPrefix;
}

void OmniboxPopupFullPresenter::WidgetDestroyed() {
  widget_observation_.Reset();
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

bool OmniboxPopupFullPresenter::ShouldUseWebContentHeight() const {
  return true;
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

    controller()->client()->FocusWebContents();
    controller()->edit_model()->OnKillFocus();
    // TODO(b/519724566): Look into using popup_closer here.
    controller()->StopAutocomplete(/*clear_result=*/true);

    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}
