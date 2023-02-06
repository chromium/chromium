// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_

#include <string>

#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"

class HoverButton;

void ConfigureBubbleMenuItem(views::Button* button, int button_id);

// Convenience method for creating a menu item used inside a bubble that can
// then be further configured to hold an image and text.
std::unique_ptr<HoverButton> CreateBubbleMenuItem(
    int button_id,
    const std::u16string& name,
    views::Button::PressedCallback callback,
    const ui::ImageModel& icon = ui::ImageModel());

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_
