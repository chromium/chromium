// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/views/toolbar/dino_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"

namespace views {
class ButtonListener;
}  // namespace views

class Browser;
class HomeButton;
class ToolbarButton;

std::unique_ptr<ToolbarButton> CreateBackButton(views::ButtonListener* listener,
                                                Browser* browser);
std::unique_ptr<ToolbarButton> CreateForwardButton(
    views::ButtonListener* listener,
    Browser* browser);
std::unique_ptr<DinoButton> CreateDinoButton(Browser* browser);
std::unique_ptr<ReloadButton> CreateReloadButton(
    Browser* browser,
    ReloadButton::IconStyle icon_style);
std::unique_ptr<HomeButton> CreateHomeButton(views::ButtonListener* listener,
                                             Browser* browser);

#if defined(OS_WIN)
// For Windows 10 and later, we use custom icons for minimal-ui web app
// Back and Reload buttons, to conform to the native OS' appearance.
// https://w3c.github.io/manifest/#dom-displaymodetype-minimal-ui
bool UseWindowsIconsForMinimalUI();
#endif

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_
