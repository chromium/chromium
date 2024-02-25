// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_CLIENT_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"

class Profile;

namespace ash::download_status {

struct DisplayMetadata;

// The virtual base class of Ash classes that display download updates.
class DisplayClient : public ProfileObserver {
 public:
  explicit DisplayClient(Profile* profile);
  DisplayClient(const DisplayClient&) = delete;
  DisplayClient& operator=(const DisplayClient&) = delete;
  ~DisplayClient() override;

  // Adds or updates the displayed download specified by `guid` with the given
  // display metadata.
  virtual void AddOrUpdate(const std::string& guid,
                           const DisplayMetadata& display_metadata) = 0;

  // Removes the displayed download specified by `guid`.
  virtual void Remove(const std::string& guid) = 0;

 protected:
  Profile* profile() { return profile_; }

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Reset when `OnProfileWillBeDestroyed()` is called to prevent the dangling
  // pointer issue.
  raw_ptr<Profile> profile_ = nullptr;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_CLIENT_H_
