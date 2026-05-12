// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_

#include "base/memory/raw_ref.h"
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
  explicit ProfileBrowserCollection(Profile* profile);
  ~ProfileBrowserCollection() override;

  static ProfileBrowserCollection* GetForProfile(Profile* profile);

  // Returns the most recently activated tabbed browser (TYPE_NORMAL) matching
  // this profile, or nullptr if no such browser exists. Browsers scheduled for
  // deletion are excluded.
  //
  // If `match_original_profiles` is true, the search includes all browsers
  // whose original profile matches this profile's original profile. This
  // covers both regular and incognito browsers for the same user.
  BrowserWindowInterface* FindTabbedBrowser(
      bool match_original_profiles = false);

  // Returns the number of off-the-record browser windows associated with this
  // profile, summed across all of its off-the-record sibling profiles (see
  // Profile::GetAllOffTheRecordProfiles()). DevTools windows are excluded on
  // non-Android.
  //
  // TODO(crbug.com/TODO): Explore removing the TYPE_DEVTOOLS exception. It
  // was inherited from the prior BrowserList implementation, but similar
  // simplifications elsewhere suggest this exception is unnecessary and is
  // an unexpected exception to clients of this API.
  size_t GetOffTheRecordBrowserCount();

 protected:
  const raw_ref<Profile> profile_;

  friend base::ScopedObservationTraits<ProfileBrowserCollection,
                                       BrowserCollectionObserver>;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_PROFILE_BROWSER_COLLECTION_H_
