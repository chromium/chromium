// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include "base/notimplemented.h"

BrowserWindowInterface* CreateBrowserWindow(
    BrowserWindowCreateParams create_params) {
  // TODO(https://crbug.com/434215969): Implement this on android.
  NOTIMPLEMENTED();
  return nullptr;
}
