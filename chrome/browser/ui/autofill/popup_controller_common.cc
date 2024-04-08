// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/popup_controller_common.h"

#include <utility>

namespace autofill {

PopupControllerCommon::PopupControllerCommon(
    gfx::RectF element_bounds,
    base::i18n::TextDirection text_direction,
    gfx::NativeView container_view)
    : element_bounds(std::move(element_bounds)),
      text_direction(text_direction),
      container_view(container_view) {}

PopupControllerCommon::PopupControllerCommon(const PopupControllerCommon&) =
    default;

PopupControllerCommon::PopupControllerCommon(PopupControllerCommon&&) = default;

PopupControllerCommon& PopupControllerCommon::operator=(
    const PopupControllerCommon&) = default;

PopupControllerCommon& PopupControllerCommon::operator=(
    PopupControllerCommon&&) = default;

PopupControllerCommon::~PopupControllerCommon() = default;

}  // namespace autofill
