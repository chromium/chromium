// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_manager_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

BrowserManagerService::BrowserManagerService(Profile* profile)
    : profile_(profile) {}

BrowserManagerService::~BrowserManagerService() = default;

void BrowserManagerService::Shutdown() {
  browsers_and_subscriptions_.clear();
}

void BrowserManagerService::AddBrowser(std::unique_ptr<Browser> browser) {
  browsers_and_subscriptions_.push_back(std::pair(
      std::move(browser),
      std::pair(browser->RegisterDidBecomeActive(base::BindRepeating(
                    &BrowserManagerService::OnBrowserActivated,
                    base::Unretained(this))),
                browser->RegisterDidBecomeInactive(base::BindRepeating(
                    &BrowserManagerService::OnBrowserDeactivated,
                    base::Unretained(this))))));

  base::WeakPtr<BrowserWindowInterface> browser_weak_ptr =
      browsers_and_subscriptions_.back().first->GetWeakPtr();
  for (BrowserCollectionObserver& observer : observers_) {
    if (browser_weak_ptr) {
      observer.OnBrowserCreated(browser_weak_ptr.get());
    }
  }
}

void BrowserManagerService::DeleteBrowser(Browser* removed_browser) {
  // Extract the Browser from `browsers_and_subscriptions_` before deleting to
  // avoid UAF risk in the case of re-entrancy.
  std::optional<BrowserAndSubscriptions> target_browser_and_subscriptions;
  for (BrowserAndSubscriptions& browser_and_subscriptions :
       browsers_and_subscriptions_) {
    if (browser_and_subscriptions.first.get() == removed_browser) {
      target_browser_and_subscriptions = std::move(browser_and_subscriptions);
      break;
    }
  }

  if (!target_browser_and_subscriptions) {
    return;
  }

  for (BrowserCollectionObserver& observer : observers_) {
    observer.OnBrowserClosed(target_browser_and_subscriptions->first.get());
  }
}

void BrowserManagerService::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : observers_) {
    observer.OnBrowserActivated(browser);
  }
}

void BrowserManagerService::OnBrowserDeactivated(
    BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : observers_) {
    observer.OnBrowserDeactivated(browser);
  }
}

void BrowserManagerService::AddObserver(BrowserCollectionObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserManagerService::RemoveObserver(
    BrowserCollectionObserver* observer) {
  observers_.RemoveObserver(observer);
}
