// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_

#include <memory>
#include <optional>
#include <string>

#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace autofill {

// The width of autofill ai bubble.
inline constexpr int kAutofillAiBubbleWidth = 320;

// Instantiate and set up a standard "edit" button.
std::unique_ptr<views::ImageButton> CreateEditButton(
    views::Button::PressedCallback callback);

// Creates an ImageModel for the Google Wallet icon.
ui::ImageModel CreateWalletIcon();

// Creates a title view for a Wallet bubble.
std::unique_ptr<views::View> CreateWalletBubbleTitleView(
    const std::u16string& title);

// Returns the inner margins for the autofill ai bubble.
gfx::Insets GetAutofillAiBubbleInnerMargins();

// Creates a view container for the autofill ai bubble subtitle.
std::unique_ptr<views::BoxLayoutView> CreateAutofillAiBubbleSubtitleContainer();

// Creates a row view displaying an attribute name and its value for autofill ai
// bubble.
// attribute_name: The name of the attribute.
// attribute_value: The value of the attribute.
// accessibility_value: Optional accessibility text for the value.
// with_blue_dot: Whether to display a blue dot next to the value.
// use_medium_font: Whether to use a medium font for the value.
std::unique_ptr<views::View> CreateAutofillAiBubbleAttributeRow(
    std::u16string attribute_name,
    std::u16string attribute_value,
    std::optional<std::u16string> accessibility_value = std::nullopt,
    bool with_blue_dot = false,
    bool use_medium_font = true);
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_
