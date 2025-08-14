// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

#include "base/android/jni_android.h"
#include "chrome/browser/ui/browser_window/internal/jni/BrowserWindowInterfaceIteratorAndroid_jni.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace {
using base::android::AttachCurrentThread;

// reinterpret_cast int64_t values to BrowserWindowInterface*.
std::vector<BrowserWindowInterface*> CastBrowserWindowPtrValues(
    const std::vector<int64_t>& browser_window_ptr_values) {
  std::vector<BrowserWindowInterface*> browser_windows;

  for (int64_t ptr_value : browser_window_ptr_values) {
    browser_windows.emplace_back(
        reinterpret_cast<BrowserWindowInterface*>(ptr_value));
  }

  return browser_windows;
}
}  // namespace

std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces() {
  std::vector<int64_t> browser_window_ptr_values =
      Java_BrowserWindowInterfaceIteratorAndroid_getAllBrowserWindowInterfaces(
          AttachCurrentThread());

  return CastBrowserWindowPtrValues(browser_window_ptr_values);
}

std::vector<BrowserWindowInterface*>
GetBrowserWindowInterfacesOrderedByActivation() {
  std::vector<int64_t> browser_window_ptr_values =
      Java_BrowserWindowInterfaceIteratorAndroid_getBrowserWindowInterfacesOrderedByActivation(
          AttachCurrentThread());

  return CastBrowserWindowPtrValues(browser_window_ptr_values);
}

void ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
    base::FunctionRef<void(BrowserWindowInterface*)> on_browser) {
  // TODO(crbug.com/438456927): This implementation does not yet account for
  // addition or removal of instances during iteration. We need a mechanism on
  // Android that is similar to chrome/browser/ui/browser_list_enumerator.h.
  std::vector<BrowserWindowInterface*> browsers =
      GetBrowserWindowInterfacesOrderedByActivation();
  for (BrowserWindowInterface* browser : browsers) {
    on_browser(browser);
  }
}

BrowserWindowInterface* GetLastActiveBrowserWindowInterfaceWithAnyProfile() {
  std::vector<BrowserWindowInterface*> browser_windows_ordered_by_activation =
      GetBrowserWindowInterfacesOrderedByActivation();
  return browser_windows_ordered_by_activation.empty()
             ? nullptr
             : browser_windows_ordered_by_activation[0];
}
