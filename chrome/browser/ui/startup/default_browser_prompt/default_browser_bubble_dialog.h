// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_BUBBLE_DIALOG_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_BUBBLE_DIALOG_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace default_browser {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kBubbleDialogSetLaterButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBubbleDialogOpenSettingsButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBubbleDialogId);

// Factory method to create and show the bubble.
std::unique_ptr<views::Widget> ShowDefaultBrowserBubbleDialog(
    views::View* anchor_view,
    bool can_pin_to_taskbar,
    base::OnceClosure on_accept,
    base::OnceClosure on_dismiss);

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_BUBBLE_DIALOG_H_
