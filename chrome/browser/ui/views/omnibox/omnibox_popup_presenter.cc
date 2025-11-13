// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"

#include <optional>

#include "base/feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/view_utils.h"

OmniboxPopupPresenter::OmniboxPopupPresenter(LocationBarView* location_bar_view,
                                             OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar_view) {
  bool full_popup =
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup);
  SetWebUIContent(
      GetUIContainer()->AddChildView(std::make_unique<OmniboxPopupWebUIContent>(
          this, this->location_bar_view(), controller,
          /*include_location_bar_cutout=*/!full_popup,
          /*wants_focus=*/full_popup)));
}

OmniboxPopupPresenter::~OmniboxPopupPresenter() = default;

bool OmniboxPopupPresenter::ShouldShowLocationBarCutout() const {
  return views::AsViewClass<OmniboxPopupWebUIContent>(GetWebUIContent())
      ->include_location_bar_cutout();
}

bool OmniboxPopupPresenter::ShouldReceiveFocus() const {
  return views::AsViewClass<OmniboxPopupWebUIContent>(GetWebUIContent())
      ->wants_focus();
}
