// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window_enumerator.h"

#include <algorithm>

#include "base/check.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

AndroidBrowserWindowEnumerator::AndroidBrowserWindowEnumerator(
    std::vector<BrowserWindowInterface*> browser_windows,
    bool enumerate_new_browser_windows)
    : browser_windows_(browser_windows),
      enumerate_new_browser_windows_(enumerate_new_browser_windows) {
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
}

AndroidBrowserWindowEnumerator::~AndroidBrowserWindowEnumerator() = default;

BrowserWindowInterface* AndroidBrowserWindowEnumerator::Next() {
  BrowserWindowInterface* browser_window = browser_windows_.front();
  browser_windows_.erase(browser_windows_.begin());
  return browser_window;
}

void AndroidBrowserWindowEnumerator::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (!enumerate_new_browser_windows_) {
    return;
  }

  DCHECK(!std::ranges::contains(browser_windows_, browser));
  browser_windows_.push_back(browser);
}

void AndroidBrowserWindowEnumerator::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  std::erase(browser_windows_, browser);
}
