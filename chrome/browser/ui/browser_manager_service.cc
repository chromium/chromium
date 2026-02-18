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
    : ProfileBrowserCollection(profile), profile_(profile) {
  AddObserver(GlobalBrowserCollection::GetInstance()->GetPlatformDelegate());
}

BrowserManagerService::~BrowserManagerService() = default;

void BrowserManagerService::Shutdown() {
  browsers_and_subscriptions_.clear();
}

bool BrowserManagerService::IsEmpty() const {
  return browsers_and_subscriptions_.empty();
}

size_t BrowserManagerService::GetSize() const {
  return browsers_and_subscriptions_.size();
}

void BrowserManagerService::AddBrowser(std::unique_ptr<Browser> browser) {
  CHECK(browsers_and_subscriptions_for_testing_.empty());
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

void BrowserManagerService::AddBrowserForTesting(
    BrowserWindowInterface* browser) {
  // Tests manually creating owned browsers must create all their instances
  // via `Browser::DeprecatedCreateOwnedForTesting()`, which calls into this
  // method.
  CHECK(browsers_and_subscriptions_.empty());
  browsers_and_subscriptions_for_testing_.push_back(
      UnownedBrowserAndSubscriptions(
          browser,
          browser->RegisterDidBecomeActive(
              base::BindRepeating(&BrowserManagerService::OnBrowserActivated,
                                  base::Unretained(this))),
          browser->RegisterDidBecomeInactive(
              base::BindRepeating(&BrowserManagerService::OnBrowserDeactivated,
                                  base::Unretained(this))),
          browser->RegisterBrowserDidClose(base::BindRepeating(
              &BrowserManagerService::OnBrowserClosedForTesting,
              base::Unretained(this)))));

  // Push the browser to the back of the activation order list. It will be moved
  // to the front when the browser is eventually activated (which may or may
  // not happen immediately after creation).
  browsers_activation_order_.push_back(browser);

  observers().Notify(&BrowserCollectionObserver::OnBrowserCreated, browser);
}

BrowserCollection::BrowserVector BrowserManagerService::GetBrowsers(
    Order order) {
  CHECK(order == Order::kCreation || order == Order::kActivation);
  if (order == Order::kActivation) {
    return browsers_activation_order_;
  }

  BrowserCollection::BrowserVector browsers;
  CHECK(browsers_and_subscriptions_.empty() ||
        browsers_and_subscriptions_for_testing_.empty());
  if (!browsers_and_subscriptions_for_testing_.empty()) {
    CHECK(browsers_and_subscriptions_.empty());
    browsers.reserve(browsers_and_subscriptions_for_testing_.size());
    std::ranges::transform(browsers_and_subscriptions_for_testing_,
                           std::back_inserter(browsers),
                           [](const auto& browser_and_subscriptions) {
                             return browser_and_subscriptions.browser.get();
                           });
  } else {
    browsers.reserve(browsers_and_subscriptions_.size());
    std::ranges::transform(browsers_and_subscriptions_,
                           std::back_inserter(browsers),
                           [](const auto& pair) { return pair.first.get(); });
  }

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

void BrowserManagerService::OnBrowserClosedForTesting(
    BrowserWindowInterface* browser) {
  // Tests manually creating owned browsers must create all their instances
  // via `Browser::DeprecatedCreateOwnedForTesting()`.
  CHECK(browsers_and_subscriptions_.empty());
  auto it = std::ranges::find_if(
      browsers_and_subscriptions_for_testing_,
      [browser](
          const UnownedBrowserAndSubscriptions& browser_and_subscriptions) {
        return browser_and_subscriptions.browser == browser;
      });
  if (it != browsers_and_subscriptions_for_testing_.end()) {
    std::erase(browsers_activation_order_, browser);
    browsers_and_subscriptions_for_testing_.erase(it);
    observers().Notify(&BrowserCollectionObserver::OnBrowserClosed, browser);
    return;
  }
}

BrowserManagerService::UnownedBrowserAndSubscriptions::
    UnownedBrowserAndSubscriptions(
        BrowserWindowInterface* browser,
        base::CallbackListSubscription activated_subscription,
        base::CallbackListSubscription deactivated_subscription,
        base::CallbackListSubscription closed_subscription)
    : browser(browser),
      activated_subscription(std::move(activated_subscription)),
      deactivated_subscription(std::move(deactivated_subscription)),
      closed_subscription(std::move(closed_subscription)) {}

BrowserManagerService::UnownedBrowserAndSubscriptions::
    UnownedBrowserAndSubscriptions(UnownedBrowserAndSubscriptions&&) = default;
