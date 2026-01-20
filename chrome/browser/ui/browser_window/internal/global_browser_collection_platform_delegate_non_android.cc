// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection_platform_delegate.h"

GlobalBrowserCollectionPlatformDelegate::
    GlobalBrowserCollectionPlatformDelegate(GlobalBrowserCollection& parent)
    : parent_(parent) {}

GlobalBrowserCollectionPlatformDelegate::
    ~GlobalBrowserCollectionPlatformDelegate() = default;

void GlobalBrowserCollectionPlatformDelegate::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  parent_->browsers_creation_order_.push_back(browser);

  // Push the browser to the back of the activation order list. It will be moved
  // to the front when the browser is eventually activated (which may or may
  // not happen immediately after creation).
  parent_->browsers_activation_order_.push_back(browser);

  for (BrowserCollectionObserver& observer : parent_->observers()) {
    observer.OnBrowserCreated(browser);
  }
}

void GlobalBrowserCollectionPlatformDelegate::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  std::erase(parent_->browsers_activation_order_, browser);
  std::erase(parent_->browsers_creation_order_, browser);
  for (BrowserCollectionObserver& observer : parent_->observers()) {
    observer.OnBrowserClosed(browser);
  }
}

void GlobalBrowserCollectionPlatformDelegate::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  // Move `browser` to the front of the activation list.
  auto it = std::ranges::find(parent_->browsers_activation_order_, browser);
  CHECK(it != parent_->browsers_activation_order_.end());
  std::rotate(parent_->browsers_activation_order_.begin(), it, it + 1);

  for (BrowserCollectionObserver& observer : parent_->observers()) {
    observer.OnBrowserActivated(browser);
  }
}

void GlobalBrowserCollectionPlatformDelegate::OnBrowserDeactivated(
    BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : parent_->observers()) {
    observer.OnBrowserDeactivated(browser);
  }
}
