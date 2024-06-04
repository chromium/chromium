// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_POPUP_CONTROLLER_COMMON_H_
#define CHROME_BROWSER_UI_AUTOFILL_POPUP_CONTROLLER_COMMON_H_

#include "base/i18n/rtl.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {

// A struct that holds some common data for Autofill style popups:
// the pop-up bounds, text direction and container view.
struct PopupControllerCommon {
 public:
  PopupControllerCommon(gfx::RectF element_bounds,
                        base::i18n::TextDirection text_direction,
                        gfx::NativeView container_view,
                        PopupAnchorType anchor_type = PopupAnchorType::kField);
  PopupControllerCommon(const PopupControllerCommon&);
  PopupControllerCommon(PopupControllerCommon&&);
  PopupControllerCommon& operator=(const PopupControllerCommon&);
  PopupControllerCommon& operator=(PopupControllerCommon&&);

  ~PopupControllerCommon();

  // The bounds of the text element that is the focus of the popup.
  // These coordinates are in screen space.
  gfx::RectF element_bounds;

  // The direction of the <input>.
  base::i18n::TextDirection text_direction;

  // Weak reference
  gfx::NativeView container_view;

  // The type of the element to anchor the popup on.
  PopupAnchorType anchor_type;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_POPUP_CONTROLLER_COMMON_H_
