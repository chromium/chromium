// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_model.h"

const size_t OmniboxPopupSelection::kNoMatch = static_cast<size_t>(-1);

bool OmniboxPopupSelection::operator==(const OmniboxPopupSelection& b) const {
  return line == b.line && state == b.state;
}

bool OmniboxPopupSelection::operator!=(const OmniboxPopupSelection& b) const {
  return !operator==(b);
}

bool OmniboxPopupSelection::operator<(const OmniboxPopupSelection& b) const {
  if (line == b.line)
    return state < b.state;

  return line < b.line;
}

bool OmniboxPopupSelection::IsChangeToKeyword(
    OmniboxPopupSelection from) const {
  return state == OmniboxPopupSelection::KEYWORD_MODE &&
         from.state != OmniboxPopupSelection::KEYWORD_MODE;
}

bool OmniboxPopupSelection::IsButtonFocused() const {
  return state != OmniboxPopupSelection::NORMAL &&
         state != OmniboxPopupSelection::KEYWORD_MODE;
}

