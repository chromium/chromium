// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_manager_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

BrowserManagerService::BrowserManagerService(Profile* profile)
    : profile_(profile) {}

BrowserManagerService::~BrowserManagerService() = default;

void BrowserManagerService::Shutdown() {
  browsers_.clear();
}

void BrowserManagerService::AddBrowser(std::unique_ptr<Browser> browser) {
  browsers_.push_back(std::move(browser));
}

void BrowserManagerService::DeleteBrowser(Browser* removed_browser) {
  // Extract the Browser from `browsers_` before deleting to avoid UAF risk in
  // the case of re-entrancy.
  std::unique_ptr<Browser> target_browser;
  for (std::unique_ptr<Browser>& browser : browsers_) {
    if (browser.get() == removed_browser) {
      target_browser = std::move(browser);
      break;
    }
  }
}
