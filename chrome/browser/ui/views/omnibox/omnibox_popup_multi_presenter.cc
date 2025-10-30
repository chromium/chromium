// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_multi_presenter.h"

#include <optional>

#include "base/feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/webui_url_constants.h"

OmniboxPopupMultiPresenter::OmniboxPopupMultiPresenter(
    LocationBarView* location_bar_view,
    OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar_view) {
  // The order of these web-views and their content must not change.
  bool full_popup =
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup);
  AddOmniboxPopupWebUIContent(controller, chrome::kChromeUIOmniboxPopupURL,
                              /*include_location_bar_cutout=*/!full_popup,
                              /*wants_focus=*/full_popup);
  AddOmniboxPopupWebUIContent(controller, chrome::kChromeUIOmniboxPopupAimURL,
                              /*include_location_bar_cutout=*/false,
                              /*wants_focus=*/true);
}

OmniboxPopupMultiPresenter::~OmniboxPopupMultiPresenter() = default;

std::optional<size_t> OmniboxPopupMultiPresenter::GetShowingWebUIContentIndex()
    const {
  return IsShown() ? webview_index_ : std::nullopt;
}

void OmniboxPopupMultiPresenter::WidgetDestroyed() {
  webview_index_.reset();
}

void OmniboxPopupMultiPresenter::ShowWebUIContent(size_t index) {
  auto* container = GetOmniboxPopupWebUIContainer();
  index = std::min(index, container->children().size() - 1);
  if (webview_index_.has_value() && (webview_index_.value() == index)) {
    return;
  }
  VLOG(4) << "ShowWebUIContent(" << index << ");";
  webview_index_ = index;
  for (size_t child_index = 0; child_index < container->children().size();
       ++child_index) {
    container->children()[child_index]->SetVisible(child_index == index);
  }
}
