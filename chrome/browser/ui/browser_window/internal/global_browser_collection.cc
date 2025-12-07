// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

#include <algorithm>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"

// static
GlobalBrowserCollection* GlobalBrowserCollection::GetInstance() {
  return g_browser_process->GetFeatures()->global_browser_collection();
}

GlobalBrowserCollection::GlobalBrowserCollection() = default;

GlobalBrowserCollection::~GlobalBrowserCollection() = default;

BrowserCollection::BrowserVector GlobalBrowserCollection::GetBrowsers(
    Order order) {
  CHECK(order == Order::kCreation || order == Order::kActivation);
  return order == Order::kCreation ? browsers_creation_order_
                                   : browsers_activation_order_;
}

void GlobalBrowserCollection::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  browsers_creation_order_.push_back(browser);

  // Push the browser to the back of the activation order list. It will be moved
  // to the front when the browser is eventually activated (which may or may
  // not happen immediately after creation).
  browsers_activation_order_.push_back(browser);

  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserCreated(browser);
  }
}

void GlobalBrowserCollection::OnBrowserClosed(BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserClosed(browser);
  }
  std::erase(browsers_activation_order_, browser);
  std::erase(browsers_creation_order_, browser);
}

void GlobalBrowserCollection::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  // Move `browser` to the front of the activation list.
  auto it = std::ranges::find(browsers_activation_order_, browser);
  CHECK(it != browsers_activation_order_.end());
  std::rotate(browsers_activation_order_.begin(), it, it + 1);

  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserActivated(browser);
  }
}

void GlobalBrowserCollection::OnBrowserDeactivated(
    BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserDeactivated(browser);
  }
}
