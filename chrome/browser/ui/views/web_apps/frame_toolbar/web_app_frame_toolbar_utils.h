// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_UTILS_H_

#include "build/build_config.h"

class ToolbarButton;
class ToolbarButtonProvider;

#if BUILDFLAG(IS_MAC)
constexpr int kWebAppMenuMargin = 7;
#endif

// Makes adjustments to |toolbar_button| for display in a web app frame.
void ConfigureWebAppToolbarButton(
    ToolbarButton* toolbar_button,
    ToolbarButtonProvider* toolbar_button_provider);

int WebAppFrameRightMargin();

int HorizontalPaddingBetweenPageActionsAndAppMenuButtons();

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_UTILS_H_
