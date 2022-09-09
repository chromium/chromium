// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/popup_controller_common.h"

namespace autofill {

PopupControllerCommon::PopupControllerCommon(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    gfx::NativeView container_view)
    : element_bounds(element_bounds),
      text_direction(text_direction),
      container_view(container_view) {}

PopupControllerCommon::~PopupControllerCommon() {}

}  // namespace autofill
