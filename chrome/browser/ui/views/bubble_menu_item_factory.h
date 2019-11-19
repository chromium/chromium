// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"

namespace views {
class Button;
class ButtonListener;
class ImageButton;
class LabelButton;
}  // namespace views

void ConfigureBubbleMenuItem(views::Button* button, int button_id);

// Convience method for creating a menu item used inside a bubble that can then
// be futher configured to hold an image and text.
std::unique_ptr<views::LabelButton> CreateBubbleMenuItem(
    int button_id,
    const base::string16& name,
    views::ButtonListener* listener);

// Convience method for creating a menu item used inside a bubble with an image.
std::unique_ptr<views::ImageButton> CreateBubbleMenuItem(
    int id,
    views::ButtonListener* listener);

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_MENU_ITEM_FACTORY_H_
