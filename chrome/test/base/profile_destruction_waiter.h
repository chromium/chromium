// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PROFILE_DESTRUCTION_WAITER_H_
#define CHROME_TEST_BASE_PROFILE_DESTRUCTION_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"

class Profile;

// Allows to wait until a profile is destroyed in tests.
class ProfileDestructionWaiter : public ProfileObserver {
 public:
  explicit ProfileDestructionWaiter(Profile* profile);
  ~ProfileDestructionWaiter() override;

  // Synchronously waits until the Profile object is about to be destroyed from
  // memory.
  void Wait();

  bool destroyed() const { return destroyed_; }

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  base::ScopedObservation<Profile, ProfileObserver> profile_observer_{this};
  bool destroyed_ = false;
  base::RunLoop run_loop_;
};

#endif  // CHROME_TEST_BASE_PROFILE_DESTRUCTION_WAITER_H_
