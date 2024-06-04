// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_OPEN_ENUMS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_OPEN_ENUMS_H_

namespace autofill {

// Specifies where the popup wil be anchoring on. This is used to define
// specific UI behaviours, for example in Desktop, when the element is `kField`
// the UI will check whether there is enough horizontal space to have a popup
// with vertical arrow (above or below the target bounds). When the element is
// `kCaret`, this check is ignored.
enum class PopupAnchorType {
  kField,
  kCaret,
  // Android suggestions are displayed anchored on the keyboard accessory.
  kKeyboardAccessory,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_OPEN_ENUMS_H_
