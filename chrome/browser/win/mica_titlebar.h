// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_MICA_TITLEBAR_H_
#define CHROME_BROWSER_WIN_MICA_TITLEBAR_H_

#include "base/feature_list.h"

BASE_DECLARE_FEATURE(kWindows11MicaTitlebar);

// Returns whether we should use the Mica titlebar in standard browser windows
// using the default theme.
bool ShouldDefaultThemeUseMicaTitlebar();

// Returns whether the system-drawn titlebar can be drawn using the Mica
// material.
bool SystemTitlebarCanUseMicaMaterial();

#endif  // CHROME_BROWSER_WIN_MICA_TITLEBAR_H_
