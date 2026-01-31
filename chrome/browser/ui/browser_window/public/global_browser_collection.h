// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection_platform_delegate.h"

// GlobalBrowserCollection is a GlobalFeature instantiated on the browser
// process at startup. It notifies its subscribed observers of all browser
// create, close, activate, and deactivate events for all browsers.
//
// If you only need to observe browsers within a single profile, use
// ProfileBrowserCollection instead.
//
// If you only need to observe a single browser, use the callback registrations
// exposed on BrowserWindowInterface instead.
//
// TODO(crbug.com/474120522): The Android implementation does not yet fire
// OnBrowserActivated and OnBrowserDeactivated BrowserCollectionObserver events.
class GlobalBrowserCollection final : public BrowserCollection {
 public:
  GlobalBrowserCollection();
  GlobalBrowserCollection(const GlobalBrowserCollection&) = delete;
  GlobalBrowserCollection& operator=(const GlobalBrowserCollection&) = delete;
  ~GlobalBrowserCollection() override;

  static GlobalBrowserCollection* GetInstance();

  // BrowserCollection:
  bool IsEmpty() const override;
  size_t GetSize() const override;

  GlobalBrowserCollectionPlatformDelegate* GetPlatformDelegate();

 protected:
  // BrowserCollection:
  BrowserVector GetBrowsers(Order order) override;

 private:
  friend base::ScopedObservationTraits<GlobalBrowserCollection,
                                       BrowserCollectionObserver>;
  friend GlobalBrowserCollectionPlatformDelegate;

  void OnBrowserCreated(BrowserWindowInterface* browser);
  void OnBrowserClosed(BrowserWindowInterface* browser);
  void OnBrowserActivated(BrowserWindowInterface* browser);
  void OnBrowserDeactivated(BrowserWindowInterface* browser);

  GlobalBrowserCollectionPlatformDelegate platform_delegate_;

  // References to the global set of browsers in creation order, with the
  // least recently created browser appearing at the front of the vector.
  std::vector<raw_ptr<BrowserWindowInterface>> browsers_creation_order_;

  // References to the global set of browsers in activation order, with the
  // most recently activated browser appearing at the front of the vector.
  std::vector<raw_ptr<BrowserWindowInterface>> browsers_activation_order_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_H_
