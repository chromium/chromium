// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include "base/functional/callback.h"
#include "base/notimplemented.h"

BrowserWindowInterface* CreateBrowserWindow(
    BrowserWindowCreateParams create_params) {
  // TODO(http://crbug.com/424860292): Implement this on Android.
  NOTIMPLEMENTED();
  return nullptr;
}

void CreateBrowserWindow(
    BrowserWindowCreateParams create_params,
    base::OnceCallback<void(BrowserWindowInterface*)> callback) {
  // TODO(http://crbug.com/424860292): Implement this on Android.
  NOTIMPLEMENTED();
}
