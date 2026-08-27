// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "ui/views/controls/button/button.h"

class GURL;

namespace views {
class ImageButton;
class View;
}  // namespace views

namespace autofill {

// Instantiate and set up a standard "edit" button.
std::unique_ptr<views::ImageButton> CreateEditButton(
    views::Button::PressedCallback callback);

// Creates a view for displaying legal message lines with clickable links.
std::unique_ptr<views::View> CreateLegalMessageView(
    const LegalMessageLines& legal_message_lines,
    base::RepeatingCallback<void(const GURL&)> callback);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_UTILS_H_
