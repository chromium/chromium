// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_
#define CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_

#include "base/feature_list.h"

class BrowserView;

// Returns whether we should custom draw the titlebar for a browser window.
bool ShouldBrowserCustomDrawTitlebar(BrowserView* browser_view);

// Returns whether we should always use the system titlebar, even when a theme
// is applied.
bool ShouldAlwaysUseSystemTitlebar();

// Returns whether we should use the Mica titlebar material for a browser
// window.
bool ShouldBrowserUseMicaTitlebar(BrowserView* browser_view);

// Returns whether we should use the Mica titlebar in standard browser windows
// using the default theme.
bool ShouldDefaultThemeUseMicaTitlebar();

// Returns whether the system-drawn titlebar can be drawn using the Mica
// material.
bool SystemTitlebarCanUseMicaMaterial();

// Returns whether the system-drawn titlebar can be drawn in dark mode.
bool SystemTitlebarSupportsDarkMode();

#endif  // CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_
