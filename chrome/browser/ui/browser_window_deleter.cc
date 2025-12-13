// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window_deleter.h"

#include "chrome/browser/ui/browser_window.h"

void BrowserWindowDeleter::operator()(BrowserWindow* browser_window) {
  browser_window->DeleteBrowserWindow();
}
