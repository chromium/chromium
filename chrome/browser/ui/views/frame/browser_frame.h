// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_

#include "chrome/browser/ui/views/frame/browser_widget.h"

// Temporary alias following BrowserFrame => BrowserWidget rename.
// Provided to give downstream projects time to update their references.
// TODO(https://crbug.com/443281722): Remove this file in a couple of weeks.
using BrowserFrame = BrowserWidget;

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
