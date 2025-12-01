// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_manager_service.h"

#include <algorithm>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

BrowserManagerService::BrowserManagerService(Profile* profile)
    : profile_(profile) {
  AddObserver(GlobalBrowserCollection::GetInstance());
}

BrowserManagerService::~BrowserManagerService() = default;

void BrowserManagerService::Shutdown() {
  browsers_and_subscriptions_.clear();
}

void BrowserManagerService::AddBrowser(std::unique_ptr<Browser> browser) {
  BrowserWindowInterface* const browser_ptr = browser.get();
  browsers_and_subscriptions_.push_back(std::pair(
      std::move(browser),
      std::pair(browser->RegisterDidBecomeActive(base::BindRepeating(
                    &BrowserManagerService::OnBrowserActivated,
                    base::Unretained(this))),
                browser->RegisterDidBecomeInactive(base::BindRepeating(
                    &BrowserManagerService::OnBrowserDeactivated,
                    base::Unretained(this))))));

  // Push the browser to the back of the activation order list. It will be moved
  // to the front when the browser is eventually activated (which may or may
  // not happen immediately after creation).
  browsers_activation_order_.push_back(browser_ptr);

  base::WeakPtr<BrowserWindowInterface> browser_weak_ptr =
      browser_ptr->GetWeakPtr();
  for (BrowserCollectionObserver& observer : observers()) {
    if (browser_weak_ptr) {
      observer.OnBrowserCreated(browser_weak_ptr.get());
    }
  }
}

void BrowserManagerService::DeleteBrowser(Browser* removed_browser) {
  // Extract the Browser from `browsers_and_subscriptions_` before deleting to
  // avoid UAF risk in the case of re-entrancy.
  std::optional<BrowserAndSubscriptions> target_browser_and_subscriptions;
  auto it = std::ranges::find_if(
      browsers_and_subscriptions_,
      [&removed_browser](
          const BrowserAndSubscriptions& browser_and_subscriptions) {
        return browser_and_subscriptions.first.get() == removed_browser;
      });
  if (it != browsers_and_subscriptions_.end()) {
    std::erase(browsers_activation_order_, it->first.get());
    target_browser_and_subscriptions = std::move(*it);
    browsers_and_subscriptions_.erase(it);
  } else {
    // `removed_browser` not present in `browsers_and_subscriptions_`.
    return;
  }

  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserClosed(target_browser_and_subscriptions->first.get());
  }
}

BrowserCollection::BrowserVector BrowserManagerService::GetBrowsers(
    Order order) {
  CHECK(order == Order::kCreation || order == Order::kActivation);
  if (order == Order::kActivation) {
    return browsers_activation_order_;
  }

  BrowserCollection::BrowserVector browsers;
  browsers.reserve(browsers_and_subscriptions_.size());
  std::ranges::transform(browsers_and_subscriptions_,
                         std::back_inserter(browsers),
                         [](const auto& pair) { return pair.first.get(); });
  return browsers;
}

void BrowserManagerService::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  // Move `browser` to the front of the activation list.
  auto it = std::ranges::find(browsers_activation_order_, browser);
  CHECK(it != browsers_activation_order_.end());
  std::rotate(browsers_activation_order_.begin(), it, it + 1);

  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserActivated(browser);
  }
}

void BrowserManagerService::OnBrowserDeactivated(
    BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserDeactivated(browser);
  }
}
