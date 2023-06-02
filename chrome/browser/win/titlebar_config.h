// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_
#define CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_

#include "base/feature_list.h"

class BrowserView;

BASE_DECLARE_FEATURE(kWindows11MicaTitlebar);

// Returns whether we should custom draw the titlebar for a browser window.
bool ShouldBrowserCustomDrawTitlebar(BrowserView* browser_view);

// Returns whether we should use the Mica titlebar in standard browser windows
// using the default theme.
bool ShouldDefaultThemeUseMicaTitlebar();

// Returns whether the system-drawn titlebar can be drawn using the Mica
// material.
bool SystemTitlebarCanUseMicaMaterial();

#endif  // CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_
