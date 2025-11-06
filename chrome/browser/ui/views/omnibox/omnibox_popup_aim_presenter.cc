// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"

#include <optional>

#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"

OmniboxPopupAimPresenter::OmniboxPopupAimPresenter(
    LocationBarView* location_bar_view,
    OmniboxController* controller)
    : OmniboxPopupPresenterBase(location_bar_view) {
  SetWebUIContent(std::make_unique<OmniboxAimPopupWebUIContent>(
      this, this->location_bar_view(), controller));
}

OmniboxPopupAimPresenter::~OmniboxPopupAimPresenter() = default;
