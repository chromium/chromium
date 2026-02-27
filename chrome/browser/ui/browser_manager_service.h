// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_H_
#define CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class BrowserWindowInterface;
class Profile;

// BrowserManagerService is responsible for owning and destroying Browser object
// instances for a given Profile.
// TODO(crbug.com/431671448): Expand this API to support
// browser_window_interface_iterator functionality (such as tracking Browsers in
// order of activation per-profile).
class BrowserManagerService : public KeyedService,
                              public ProfileBrowserCollection {
 public:
  explicit BrowserManagerService(Profile* profile);
  ~BrowserManagerService() override;

  // KeyedService:
  void Shutdown() override;

  // BrowserCollection:
  bool IsEmpty() const override;
  size_t GetSize() const override;

  // Adds a new Browser to be owned by the service.
  void AddBrowser(std::unique_ptr<Browser> browser);

  // Destroys `browser` if owned and managed by the service. It is expected that
  // `browser` has first emitted a did-close event and `OnBrowserClosed()` is
  // called before `DeleteBrowser()`.
  void DeleteBrowser(Browser* browser);

  // Adds a new unowned Browser created by unit tests.
  // TODO(crbug.com/417766643): Remove this once all use of Browser in unit
  // tests has been eliminated.
  void AddBrowserForTesting(BrowserWindowInterface* browser);

 protected:
  // ProfileBrowserCollection:
  BrowserVector GetBrowsers(Order order) override;

 private:
  // Browser event listeners ---------------------------------------------------
  // This service owns and manages browser objects associated with its profile.
  // The browser objects themselves emit browser-specific events such as
  // activation / deactivation / window-close. This service listens for these
  // events on each browser instance it manages and aggregates the events
  // for propagation to collection observers.

  // Triggered by events emitted by browsers managed by this service. Triggered
  // when a browser's backing window becomes active.
  void OnBrowserActivated(BrowserWindowInterface* browser);

  // Triggered by events emitted by browsers managed by this service. Triggered
  // when a browser's backing window becomes inactive.
  void OnBrowserDeactivated(BrowserWindowInterface* browser);

  // Triggered by events emitted by browsers managed by this service. Triggered
  // when a browser has closed (i.e. all tabs are destroyed and the browser
  // enters a pending-delete state, after which it is destroyed asynchronously
  // via a call into `DeleteBrowser()`).
  void OnBrowserClosed(BrowserWindowInterface* browser);

  // Called when browsers in `browsers_and_subscriptions_for_testing_` have
  // closed. Removes `browser` from `browsers_and_subscriptions_for_testing_`.
  void OnBrowserClosedForTesting(BrowserWindowInterface* browser);

  // ---------------------------------------------------------------------------

  // References to browsers owned by the service in activation order, with the
  // most recently activated browser appearing at the front of the vector.
  std::vector<raw_ptr<BrowserWindowInterface>> browsers_activation_order_;

  // We need to hold subscriptions for each Browser. Stores the browser in
  // creation order, with the least recently created browser appearing at the
  // front of the vector.
  struct BrowserAndSubscriptions {
    BrowserAndSubscriptions(
        std::unique_ptr<BrowserWindowInterface> browser,
        base::CallbackListSubscription activated_subscription,
        base::CallbackListSubscription deactivated_subscription,
        base::CallbackListSubscription closed_subscription);
    BrowserAndSubscriptions(const BrowserAndSubscriptions&) = delete;
    BrowserAndSubscriptions& operator=(const BrowserAndSubscriptions&) = delete;
    BrowserAndSubscriptions(BrowserAndSubscriptions&&);
    BrowserAndSubscriptions& operator=(BrowserAndSubscriptions&&) = default;
    ~BrowserAndSubscriptions();

    // ORDER MATTERS: `browser` is declared last so it is destroyed
    // first. This ensures that when `~Browser()` fires close callbacks,
    // the subscriptions are still alive.
    base::CallbackListSubscription activated_subscription;
    base::CallbackListSubscription deactivated_subscription;
    base::CallbackListSubscription closed_subscription;
    std::unique_ptr<BrowserWindowInterface> browser;
  };
  std::vector<BrowserAndSubscriptions> browsers_and_subscriptions_;

  // `browsers_and_subscriptions_for_testing_` and `browsers_and_subscriptions_`
  // are mutually exclusive to each other. Tests creating owned test Browser
  // instances should never be creating unowned Browser instances.
  // TODO(crbug.com/417766643): Remove this once all use of Browser in unit
  // tests has been eliminated.
  struct UnownedBrowserAndSubscriptions {
    UnownedBrowserAndSubscriptions(
        BrowserWindowInterface* browser,
        base::CallbackListSubscription activated_subscription,
        base::CallbackListSubscription deactivated_subscription,
        base::CallbackListSubscription closed_subscription);
    UnownedBrowserAndSubscriptions(const UnownedBrowserAndSubscriptions&) =
        delete;
    UnownedBrowserAndSubscriptions& operator=(
        const UnownedBrowserAndSubscriptions&) = delete;
    UnownedBrowserAndSubscriptions(UnownedBrowserAndSubscriptions&&);
    UnownedBrowserAndSubscriptions& operator=(
        UnownedBrowserAndSubscriptions&&) = default;
    ~UnownedBrowserAndSubscriptions() = default;

    raw_ptr<BrowserWindowInterface> browser;
    base::CallbackListSubscription activated_subscription;
    base::CallbackListSubscription deactivated_subscription;
    base::CallbackListSubscription closed_subscription;
  };
  std::vector<UnownedBrowserAndSubscriptions>
      browsers_and_subscriptions_for_testing_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_H_
