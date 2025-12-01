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

  // Adds a new Browser to be owned by the service.
  void AddBrowser(std::unique_ptr<Browser> browser);

  // Destroys `browser` if owned and managed by the service.
  void DeleteBrowser(Browser* browser);

 protected:
  // ProfileBrowserCollection:
  BrowserVector GetBrowsers(Order order) override;

 private:
  // Called when a browser in this profile became active.
  void OnBrowserActivated(BrowserWindowInterface* browser);

  // Called when a browser in this profile became inactive.
  void OnBrowserDeactivated(BrowserWindowInterface* browser);

  // Profile associated with this service.
  const raw_ptr<Profile> profile_;

  // References to browsers owned by the service in activation order, with the
  // most recently activated browser appearing at the front of the vector.
  std::vector<raw_ptr<BrowserWindowInterface>> browsers_activation_order_;

  // We need to hold 2 subscriptions for each Browser: one for DidBecomeActive
  // and one for DidBecomeInactive. Stores the browser in creation order, with
  // the least recently created browser appearing at the front of the vector.
  using BrowserAndSubscriptions =
      std::pair<std::unique_ptr<Browser>,
                std::pair<base::CallbackListSubscription,
                          base::CallbackListSubscription>>;
  std::vector<BrowserAndSubscriptions> browsers_and_subscriptions_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_H_
