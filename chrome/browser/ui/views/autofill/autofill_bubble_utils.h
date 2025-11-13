// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_

#include <memory>
#include <string>

#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace autofill {

// Instantiate and set up a standard "edit" button.
std::unique_ptr<views::ImageButton> CreateEditButton(
    views::Button::PressedCallback callback);

// Creates an ImageModel for the Google Wallet icon.
ui::ImageModel CreateWalletIcon();

// Creates a title view for a Wallet bubble.
std::unique_ptr<views::View> CreateWalletBubbleTitleView(
    const std::u16string& title);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_
