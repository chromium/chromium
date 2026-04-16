// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/popup_controller_common.h"

#include <utility>

#include "components/autofill/core/browser/ui/popup_open_enums.h"

namespace autofill {

PopupControllerCommon::PopupControllerCommon(
    gfx::RectF element_bounds,
    base::i18n::TextDirection text_direction,
    gfx::NativeView container_view,
    PopupAnchorType anchor_type,
    bool show_tabbed_popup,
    bool prefer_prev_arrow_side_on_suggestions_update)
    : element_bounds(std::move(element_bounds)),
      text_direction(text_direction),
      container_view(container_view),
      anchor_type(anchor_type),
      show_tabbed_popup(show_tabbed_popup),
      prefer_prev_arrow_side_on_suggestions_update(
          prefer_prev_arrow_side_on_suggestions_update) {}

PopupControllerCommon::PopupControllerCommon(const PopupControllerCommon&) =
    default;

PopupControllerCommon::PopupControllerCommon(PopupControllerCommon&&) = default;

PopupControllerCommon& PopupControllerCommon::operator=(
    const PopupControllerCommon&) = default;

PopupControllerCommon& PopupControllerCommon::operator=(
    PopupControllerCommon&&) = default;

PopupControllerCommon::~PopupControllerCommon() = default;

}  // namespace autofill
