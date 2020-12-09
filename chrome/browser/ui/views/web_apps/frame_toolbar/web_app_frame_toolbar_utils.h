// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_UTILS_H_

#include "build/build_config.h"

class ToolbarButton;

#if defined(OS_MAC)
constexpr int kWebAppMenuMargin = 7;
#endif

// An ink drop with round corners in shown when the user hovers over the button.
// Insets are kept small to avoid increasing web app frame toolbar height.
void SetInsetsForWebAppToolbarButton(ToolbarButton* toolbar_button,
                                     bool is_browser_focus_mode);

int WebAppFrameRightMargin();

int HorizontalPaddingBetweenPageActionsAndAppMenuButtons();

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_UTILS_H_
