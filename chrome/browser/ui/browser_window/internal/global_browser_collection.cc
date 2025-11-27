// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"

// static
GlobalBrowserCollection* GlobalBrowserCollection::GetInstance() {
  return g_browser_process->GetFeatures()->global_browser_collection();
}

GlobalBrowserCollection::GlobalBrowserCollection() = default;

GlobalBrowserCollection::~GlobalBrowserCollection() = default;

void GlobalBrowserCollection::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserCreated(browser);
  }
}

void GlobalBrowserCollection::OnBrowserClosed(BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserClosed(browser);
  }
}

void GlobalBrowserCollection::OnBrowserActivated(
    BrowserWindowInterface* browser) {
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
