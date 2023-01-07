// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_
#define CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_

#include "base/feature_list.h"

BASE_DECLARE_FEATURE(kWindows10CustomTitlebar);

// Returns whether we should custom draw the titlebar even if we're using the
// native frame.
bool ShouldCustomDrawSystemTitlebar();

#endif  // CHROME_BROWSER_WIN_TITLEBAR_CONFIG_H_
