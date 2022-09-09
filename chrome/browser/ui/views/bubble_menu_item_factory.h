// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_

#include <memory>
#include <string>

#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"

class HoverButton;

void ConfigureBubbleMenuItem(views::Button* button, int button_id);

// Convenience method for creating a menu item used inside a bubble that can
// then be further configured to hold an image and text.
std::unique_ptr<HoverButton> CreateBubbleMenuItem(
    int button_id,
    const std::u16string& name,
    views::Button::PressedCallback callback,
    const gfx::VectorIcon* icon = nullptr);

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_
