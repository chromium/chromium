// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"

#include <optional>

#include "base/feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/webui_url_constants.h"

OmniboxPopupPresenter::OmniboxPopupPresenter(LocationBarView* location_bar_view,
                                             OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar_view) {
  bool full_popup =
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup);
  AddOmniboxPopupWebUIContent(controller, chrome::kChromeUIOmniboxPopupURL,
                              /*include_location_bar_cutout=*/!full_popup,
                              /*wants_focus=*/full_popup);
}

OmniboxPopupPresenter::~OmniboxPopupPresenter() = default;

std::optional<size_t> OmniboxPopupPresenter::GetShowingWebUIContentIndex()
    const {
  if (IsShown()) {
    return 0;
  }
  return std::nullopt;
}

void OmniboxPopupPresenter::ShowWebUIContent(size_t index) {
  GetOmniboxPopupWebUIContainer()->children().front()->SetVisible(true);
}
