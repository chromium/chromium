// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"

#include <optional>

#include "base/feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/view_utils.h"

OmniboxPopupPresenter::OmniboxPopupPresenter(LocationBarView* location_bar_view,
                                             OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar_view) {
  bool full_popup =
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup);
  SetWebUIContent(std::make_unique<OmniboxPopupWebUIContent>(
      this, this->location_bar_view(), controller,
      /*include_location_bar_cutout=*/!full_popup,
      /*wants_focus=*/full_popup));
}

OmniboxPopupPresenter::~OmniboxPopupPresenter() = default;

void OmniboxPopupPresenter::Hide() {
  OmniboxPopupPresenterBase::Hide();
  // Note: This makes the widget and webview have heights that are not synced.
  // So if the webview content height does not change (No resize event happens)
  // we need to manually call OnContentHeightChanged to update this value.
  content_height_ = 1;
}

void OmniboxPopupPresenter::WidgetDestroyed() {
  // Update the popup state manager if widget was destroyed externally, e.g., by
  // the OS. This ensures the popup state manager stays in sync.
  auto* controller = location_bar_view()->GetOmniboxController();
  if (controller->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kClassic) {
    controller->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);
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
