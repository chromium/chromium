// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_

#include "base/scoped_observation_traits.h"

class BrowserCollectionObserver;
class Profile;

// ProfileBrowserCollection notifies its subscribed observers of all browser
// create, close, activate, and deactivate events for the associated profile.
class ProfileBrowserCollection {
 public:
  static ProfileBrowserCollection* GetForProfile(Profile* profile);

  virtual ~ProfileBrowserCollection() = default;

 private:
  // Use base::ScopedObservationTraits to register as an observer instead of
  // calling these directly.
  virtual void AddObserver(BrowserCollectionObserver* observer) = 0;
  virtual void RemoveObserver(BrowserCollectionObserver* observer) = 0;

  // Allow access to private AddObserver() and RemoveObserver() functions.
  friend base::ScopedObservationTraits<ProfileBrowserCollection,
                                       BrowserCollectionObserver>;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_
