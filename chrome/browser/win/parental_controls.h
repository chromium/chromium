// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_PARENTAL_CONTROLS_H_
#define CHROME_BROWSER_WIN_PARENTAL_CONTROLS_H_

#include "base/compiler_specific.h"

struct WinParentalControls {
  bool any_restrictions = false;
  bool logging_required = false;
  bool web_filter = false;
};

// Calculates and caches the platform parental controls on a worker thread.
void InitializeWinParentalControls();

// Returns a struct of enabled parental controls. This method evaluates and
// caches if the platform controls have been enabled on the first call, which
// requires a thread supporting blocking. Subsequent calls may be from any
// thread.
const WinParentalControls& GetWinParentalControls() WARN_UNUSED_RESULT;

#endif  // CHROME_BROWSER_WIN_PARENTAL_CONTROLS_H_
