// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/root_element.h"

#include "base/notreached.h"
#include "components/ui_devtools/protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"

namespace ui_devtools {

RootElement::RootElement(UIElementDelegate* ui_element_delegate)
    : UIElement(UIElementType::ROOT, ui_element_delegate, nullptr) {}

RootElement::~RootElement() = default;

void RootElement::GetBounds(gfx::Rect* bounds) const {
  NOTREACHED_IN_MIGRATION();
}

void RootElement::SetBounds(const gfx::Rect& bounds) {
  NOTREACHED_IN_MIGRATION();
}

void RootElement::GetVisible(bool* visible) const {
  NOTREACHED_IN_MIGRATION();
}

void RootElement::SetVisible(bool visible) {
  NOTREACHED_IN_MIGRATION();
}

std::vector<std::string> RootElement::GetAttributes() const {
  NOTREACHED_IN_MIGRATION();
  return {};
}

std::pair<gfx::NativeWindow, gfx::Rect>
RootElement::GetNodeWindowAndScreenBounds() const {
  NOTREACHED_IN_MIGRATION();
  return {};
}

}  // namespace ui_devtools
