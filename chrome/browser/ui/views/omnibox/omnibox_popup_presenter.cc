// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/views/view_utils.h"

OmniboxPopupPresenter::OmniboxPopupPresenter(
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate,
    OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar, presenter_delegate, controller) {
  bool full_popup =
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup);
  SetWebUIContent(std::make_unique<OmniboxPopupWebUIContent>(
      this, this->location_bar(), controller,
      /*include_location_bar_cutout=*/!full_popup,
      /*wants_focus=*/full_popup));

  // By initializing `content_height_` to 1, we ensure the widget starts 1px
  // taller than the location bar on first show. This creates a tiny visible
  // area that forces the renderer to run layout and submit a frame (carrying
  // size metadata) instead of skipping it. This ensures auto-resizes are
  // triggered reliably.
  // Only needed if `kOmniboxWebUIDeferShowUntilVisualStateReady` is disabled,
  // as waiting for the visual state callback fixes the issue.
  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady)) {
    content_height_ = 1;
  }
}

OmniboxPopupPresenter::~OmniboxPopupPresenter() = default;

void OmniboxPopupPresenter::Hide() {
  OmniboxPopupPresenterBase::Hide();
  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady)) {
    // Reset the cached height to force a layout update when the popup is
    // reshown. This prevents the popup from temporarily using a stale size
    // from its previous state.
    content_height_ = 1;
  }
}

std::string_view OmniboxPopupPresenter::GetPopupMetricPrefix() const {
  return OmniboxPopupPresenterBase::kWebUIPopupMetricPrefix;
}

void OmniboxPopupPresenter::WidgetDestroyed() {
  // Update the popup state manager if widget was destroyed externally, e.g., by
  // the OS. This ensures the popup state manager stays in sync.
  if (controller()->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kClassic) {
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

bool OmniboxPopupPresenter::ShouldShowLocationBarCutout() const {
  return views::AsViewClass<OmniboxPopupWebUIContent>(GetWebUIContent())
      ->include_location_bar_cutout();
}

bool OmniboxPopupPresenter::ShouldReceiveFocus() const {
  return views::AsViewClass<OmniboxPopupWebUIContent>(GetWebUIContent())
      ->wants_focus();
}

std::optional<base::TimeDelta>
OmniboxPopupPresenter::ShouldDeferUntilVisualStateReady() const {
  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady)) {
    return std::nullopt;
  }
  return base::Milliseconds(
      omnibox::kOmniboxWebUIDeferShowUntilVisualStateReadyTimeoutMs.Get());
}

bool OmniboxPopupPresenter::ShouldDetachWebContentsOnHide() const {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxWebUIDetachWebContentsOnHide);
}
