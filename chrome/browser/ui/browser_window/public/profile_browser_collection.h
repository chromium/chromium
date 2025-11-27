// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_

#include "base/scoped_observation_traits.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"

class Profile;

// ProfileBrowserCollection notifies its subscribed observers of all browser
// create, close, activate, and deactivate events for the associated profile.
//
// If you only need to observe a single browser, use the callback registrations
// exposed on BrowserWindowInterface instead.
class ProfileBrowserCollection : public BrowserCollection {
 public:
  ~ProfileBrowserCollection() override = default;

  static ProfileBrowserCollection* GetForProfile(Profile* profile);

 private:
  friend base::ScopedObservationTraits<ProfileBrowserCollection,
                                       BrowserCollectionObserver>;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_
