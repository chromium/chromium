// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_PROFILE_LAUNCH_OBSERVER_H_
#define CHROME_BROWSER_UI_STARTUP_PROFILE_LAUNCH_OBSERVER_H_

#include <set>

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

class BrowserWindowInterface;
class Profile;
class GlobalBrowserCollection;

// Keeps track on which profiles have been launched. This should not be
// instantiated outside of profile_launch_observer.cc - only use static methods.
class ProfileLaunchObserver : public ProfileObserver,
                              public BrowserCollectionObserver {
 public:
  ProfileLaunchObserver();
  ProfileLaunchObserver(const ProfileLaunchObserver&) = delete;
  ProfileLaunchObserver& operator=(const ProfileLaunchObserver&) = delete;
  ~ProfileLaunchObserver() override;

  static void AddLaunched(Profile* profile);

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  friend class StartupBrowserCreator;
  friend bool HasPendingUncleanExit(Profile*);

  // Only for ProfileLaunchObserver.
  static ProfileLaunchObserver* GetInstance();

  // Only for StartupBrowserCreator.
  static void ClearForTesting();
  static void set_profile_to_activate(Profile* profile);
  static bool activated_profile();

  // Only for HasPendingUncleanExit().
  static bool HasBeenLaunchedAndBrowserOpen(const Profile* profile);

  // Returns true if `profile` has been launched by
  // StartupBrowserCreator::LaunchBrowser() and has at least one open window.
  bool HasBeenLaunchedAndBrowserOpenInternal(const Profile* profile) const;
  void AddLaunchedInternal(Profile* profile);
  void Clear();
  bool activated_profile_internal();
  void set_profile_to_activate_internal(Profile* profile);

  void MaybeActivateProfile();
  void ActivateProfile();

  // These are the profiles that get launched by
  // StartupBrowserCreator::LaunchBrowser.
  std::set<raw_ptr<const Profile, SetExperimental>> launched_profiles_;
  // These are the profiles for which at least one browser window has been
  // opened. This is needed to know when it is safe to activate
  // |profile_to_activate_|, otherwise, new browser windows being opened will
  // be activated on top of it.
  std::set<raw_ptr<const Profile, SetExperimental>> opened_profiles_;
  // This is null until the profile to activate has been chosen. This value
  // should only be set once all profiles have been launched, otherwise,
  // activation may not happen after the launch of newer profiles.
  raw_ptr<Profile, DanglingUntriaged> profile_to_activate_ = nullptr;
  // Set once we attempted to activate a profile. We only get one shot at this.
  bool activated_profile_ = false;
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_STARTUP_PROFILE_LAUNCH_OBSERVER_H_
