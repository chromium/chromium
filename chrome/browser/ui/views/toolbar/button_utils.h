// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_

#include <memory>

namespace views {
class ButtonListener;
}  // namespace views

class Browser;
class HomeButton;
class ReloadButton;
class ToolbarButton;

std::unique_ptr<ToolbarButton> CreateBackButton(views::ButtonListener* listener,
                                                Browser* browser);
std::unique_ptr<ToolbarButton> CreateForwardButton(
    views::ButtonListener* listener,
    Browser* browser);
std::unique_ptr<ReloadButton> CreateReloadButton(Browser* browser);
std::unique_ptr<HomeButton> CreateHomeButton(views::ButtonListener* listener,
                                             Browser* browser);

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_
