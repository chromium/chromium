// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TAB_COUNTER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TAB_COUNTER_BUTTON_H_

#include <memory>

#include "ui/views/controls/button/button.h"

namespace views {
class View;
}  // namespace views

class BrowserView;

std::unique_ptr<views::View> CreateWebUITabCounterButton(
    views::Button::PressedCallback pressed_callback,
    BrowserView* browser_view);

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TAB_COUNTER_BUTTON_H_
