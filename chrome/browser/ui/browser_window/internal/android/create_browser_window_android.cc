// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"

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

BrowserWindowInterface::CreationStatus GetBrowserWindowCreationStatusForProfile(
    Profile& profile) {
  if (profile.ShutdownStarted()) {
    return BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable;
  }

  return BrowserWindowInterface::CreationStatus::kOk;
}
