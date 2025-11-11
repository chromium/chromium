// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_H_

#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

// GlobalBrowserCollection is a GlobalFeature instantiated on the browser
// process at startup. It notifies its subscribed observers of all browser
// create, close, activate, and deactivate events for all browsers.
//
// If you only need to observe browsers within a single profile, use
// ProfileBrowserCollection instead.
//
// If you only need to observe a single browser, use the callback registrations
// exposed on BrowserWindowInterface instead.
class GlobalBrowserCollection final : public BrowserCollectionObserver {
 public:
  static GlobalBrowserCollection* GetInstance();

  GlobalBrowserCollection();
  GlobalBrowserCollection(const GlobalBrowserCollection&) = delete;
  GlobalBrowserCollection& operator=(const GlobalBrowserCollection&) = delete;
  ~GlobalBrowserCollection() override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override;

 private:
  // Use base::ScopedObservationTraits to register as an observer instead of
  // calling these directly.
  void AddObserver(BrowserCollectionObserver* observer);
  void RemoveObserver(BrowserCollectionObserver* observer);

  // A list of observers which will be notified of every browser addition and
  // removal across all browsers globally.
  base::ObserverList<BrowserCollectionObserver> observers_;

  // Allow access to private AddObserver() and RemoveObserver() functions.
  friend base::ScopedObservationTraits<GlobalBrowserCollection,
                                       BrowserCollectionObserver>;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_H_
