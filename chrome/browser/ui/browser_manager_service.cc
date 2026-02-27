// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_manager_service.h"

#include <algorithm>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "printing/buildflags/buildflags.h"

BrowserManagerService::BrowserManagerService(Profile* profile)
    : ProfileBrowserCollection(profile) {
  AddObserver(GlobalBrowserCollection::GetInstance()->GetPlatformDelegate());
}

BrowserManagerService::~BrowserManagerService() = default;

void BrowserManagerService::Shutdown() {
  CHECK(browsers_and_subscriptions_for_testing_.empty());

  while (!browsers_and_subscriptions_.empty()) {
    BrowserAndSubscriptions entry =
        std::move(browsers_and_subscriptions_.back());
    browsers_and_subscriptions_.pop_back();
    std::erase(browsers_activation_order_, entry.browser.get());

    // `entry` is destroyed here. Member destruction order ensures browser
    // is released before subscriptions are destroyed.
  }

  browsers_activation_order_.clear();
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
  // Prefer push_back, see totw/112.
  // NOLINTNEXTLINE(modernize-use-emplace)
  browsers_and_subscriptions_.push_back(BrowserAndSubscriptions(
      std::move(browser),
      browser_ptr->RegisterDidBecomeActive(base::BindRepeating(
          &BrowserManagerService::OnBrowserActivated, base::Unretained(this))),
      browser_ptr->RegisterDidBecomeInactive(
          base::BindRepeating(&BrowserManagerService::OnBrowserDeactivated,
                              base::Unretained(this))),
      browser_ptr->RegisterBrowserDidClose(base::BindRepeating(
          &BrowserManagerService::OnBrowserClosed, base::Unretained(this)))));

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
        return browser_and_subscriptions.browser.get() == removed_browser;
      });
  if (it != browsers_and_subscriptions_.end()) {
    std::erase(browsers_activation_order_, it->browser.get());
    target_browser_and_subscriptions = std::move(*it);
    browsers_and_subscriptions_.erase(it);
  } else {
    // `removed_browser` not present in `browsers_and_subscriptions_`.
    return;
  }

  // The system incognito profile should not try be destroyed using
  // ProfileDestroyer::DestroyProfileWhenAppropriate(). This profile can be
  // used, at least, by the user manager window. This window is not a browser,
  // therefore, chrome::IsOffTheRecordBrowserActiveForProfile(profile_) returns
  // false, while the user manager window is still opened. This cannot be fixed
  // in ProfileDestroyer::DestroyProfileWhenAppropriate(), because the
  // ProfileManager needs to be able to destroy all profiles when it is
  // destroyed. See crbug.com/527035
  //
  // Non-primary OffTheRecord profiles should not be destroyed directly by
  // Browser (e.g. for offscreen tabs, https://crbug.com/664351).
  //
  // TODO(crbug.com/40159237): Use ScopedProfileKeepAlive for Incognito too,
  // instead of separate logic for Incognito and regular profiles.
  target_browser_and_subscriptions->browser.reset();
  if (browsers_and_subscriptions_.empty() && profile_->IsIncognitoProfile() &&
      !profile_->IsSystemProfile()) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    // The Printing Background Manager holds onto preview dialog WebContents
    // whose corresponding print jobs have not yet fully spooled. Make sure
    // these get destroyed before tearing down the incognito profile so that
    // their RenderFrameHosts can exit in time - see crbug.com/579155
    g_browser_process->background_printing_manager()
        ->DeletePreviewContentsForBrowserContext(&profile_.get());
#endif
    // An incognito profile is no longer needed, this indirectly frees
    // its cache and cookies once it gets destroyed at the appropriate time.
    ProfileDestroyer::DestroyOTRProfileWhenAppropriate(&profile_.get());
  }
}

void BrowserManagerService::AddBrowserForTesting(
    BrowserWindowInterface* browser) {
  // Tests manually creating owned browsers must create all their instances
  // via `Browser::DeprecatedCreateOwnedForTesting()`, which calls into this
  // method.
  CHECK(browsers_and_subscriptions_.empty());
  // Prefer push_back, see totw/112.
  // NOLINTNEXTLINE(modernize-use-emplace)
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
                           [](const auto& browser_and_subscriptions) {
                             return browser_and_subscriptions.browser.get();
                           });
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

void BrowserManagerService::OnBrowserClosed(BrowserWindowInterface* browser) {
  for (BrowserCollectionObserver& observer : observers()) {
    observer.OnBrowserClosed(browser);
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

BrowserManagerService::BrowserAndSubscriptions::BrowserAndSubscriptions(
    std::unique_ptr<BrowserWindowInterface> browser,
    base::CallbackListSubscription activated_subscription,
    base::CallbackListSubscription deactivated_subscription,
    base::CallbackListSubscription closed_subscription)
    : activated_subscription(std::move(activated_subscription)),
      deactivated_subscription(std::move(deactivated_subscription)),
      closed_subscription(std::move(closed_subscription)),
      browser(std::move(browser)) {}

BrowserManagerService::BrowserAndSubscriptions::BrowserAndSubscriptions(
    BrowserAndSubscriptions&&) = default;

BrowserManagerService::BrowserAndSubscriptions::~BrowserAndSubscriptions() =
    default;

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
