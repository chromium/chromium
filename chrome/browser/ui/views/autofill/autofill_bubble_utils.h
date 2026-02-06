// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_

#include <memory>

#include "ui/views/controls/button/button.h"

namespace views {
class ImageButton;
}  // namespace views

namespace autofill {

// Instantiate and set up a standard "edit" button.
std::unique_ptr<views::ImageButton> CreateEditButton(
    views::Button::PressedCallback callback);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_
