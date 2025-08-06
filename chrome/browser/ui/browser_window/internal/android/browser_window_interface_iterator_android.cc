// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

#include "base/android/jni_android.h"
#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"
#include "chrome/browser/ui/browser_window/internal/jni/BrowserWindowInterfaceIteratorAndroid_jni.h"

namespace {
using base::android::AttachCurrentThread;
}  // namespace

std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces() {
  std::vector<int64_t> browser_window_ptr_values =
      Java_BrowserWindowInterfaceIteratorAndroid_getAllBrowserWindowInterfaces(
          AttachCurrentThread());

  std::vector<BrowserWindowInterface*> browser_windows;
  for (int64_t ptr_value : browser_window_ptr_values) {
    browser_windows.emplace_back(
        reinterpret_cast<BrowserWindowInterface*>(ptr_value));
  }

  return browser_windows;
}

std::vector<BrowserWindowInterface*>
GetBrowserWindowInterfacesOrderedByActivation() {
  // TODO(https://crbug.com/419057482): This is wrong, since this is creation
  // order, rather than activation order. This is a temporary solution so things
  // don't crash and "mostly" work (especially when creation order matches
  // activation order, such in the case of a single window).
  return AndroidBrowserWindow::GetAllAndroidBrowserWindowsByCreationTime();
}

BrowserWindowInterface* GetLastActiveBrowserWindowInterfaceWithAnyProfile() {
  std::vector<BrowserWindowInterface*> all_windows =
      GetBrowserWindowInterfacesOrderedByActivation();
  return all_windows.empty() ? nullptr : all_windows[0];
}
