// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"

#include <string_view>

#include "chrome/browser/ui/omnibox/omnibox_controller.h"

OmniboxPopupView::OmniboxPopupView(OmniboxController* controller)
    : controller_(controller) {}

OmniboxPopupView::~OmniboxPopupView() = default;

OmniboxController* OmniboxPopupView::controller() {
  return const_cast<OmniboxController*>(
      const_cast<const OmniboxPopupView*>(this)->controller());
}

const OmniboxController* OmniboxPopupView::controller() const {
  return controller_;
}

std::u16string_view OmniboxPopupView::GetAccessibleButtonTextForResult(
    size_t line) const {
  return {};
}
