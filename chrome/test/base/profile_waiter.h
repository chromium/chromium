// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PROFILE_WAITER_H_
#define CHROME_TEST_BASE_PROFILE_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

class Profile;

// Allows to wait until a new profile is created in tests.
// This waiter works only for normal (on-the-record) profiles, as it observes
// the ProfileManager that doesn't own OTR profiles.
class ProfileWaiter : public ProfileManagerObserver {
 public:
  ProfileWaiter();
  ~ProfileWaiter() override;

  // Synchronously waits until the profile is added to ProfileManager and
  // returns the newly added Profile.
  Profile* WaitForProfileAdded();

 private:
  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};
  base::RunLoop run_loop_;
};

#endif  // CHROME_TEST_BASE_PROFILE_WAITER_H_
