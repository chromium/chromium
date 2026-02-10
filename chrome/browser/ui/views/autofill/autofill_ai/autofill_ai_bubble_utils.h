// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_BUBBLE_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_BUBBLE_UTILS_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ui/views/style/typography.h"

namespace gfx {
class Insets;
}  // namespace gfx

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class BoxLayoutView;
class View;
}  // namespace views

namespace autofill {

// The width of an Autofill AI bubble.
inline constexpr int kAutofillAiBubbleWidth = 320;

// Creates an ImageModel for the Google Wallet icon.
ui::ImageModel CreateWalletIcon();

// Creates a title view for a Wallet bubble.
std::unique_ptr<views::View> CreateWalletBubbleTitleView(std::u16string title);

// Returns the inner margins for the autofill ai bubble.
gfx::Insets GetAutofillAiBubbleInnerMargins();

// Creates a view container for the autofill ai bubble subtitle.
std::unique_ptr<views::BoxLayoutView> CreateAutofillAiBubbleSubtitleContainer();

// Creates a row view displaying an attribute name and its value for an Autofill
// AI bubble.
// - `attribute_name`, the name of the attribute.
// - `new_attribute_value`, the new value of the attribute.
// - `old_attribute_value`, the previous value of the attribute (if this is an
//    update).
// - `accessibility_value`,  the accessibility text for the value.
// - `value_font_style`, the font style to use for the new value label.
// - `with_blue_dot`, whether to display a blue dot next to the value
std::unique_ptr<views::View> CreateAutofillAiBubbleAttributeRow(
    std::u16string attribute_name,
    std::u16string new_attribute_value,
    std::optional<std::u16string> old_attribute_value = std::nullopt,
    std::optional<std::u16string> accessibility_value = std::nullopt,
    int new_value_font_style = views::style::STYLE_BODY_4_MEDIUM,
    bool with_blue_dot = true);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_BUBBLE_UTILS_H_
