// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROFILE_DELETION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROFILE_DELETION_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

class ProfileManager;
class Profile;

namespace web_app {

class WebAppProvider;

class WebAppProfileDeletionManager : public ProfileManagerObserver {
 public:
  explicit WebAppProfileDeletionManager(Profile* profile);
  ~WebAppProfileDeletionManager() override;

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Start();
  void Shutdown();

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(
      Profile* profile_to_be_deleted) override;
  void OnProfileManagerDestroying() override;

 private:
  void RemoveDataForProfileDeletion();

  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROFILE_DELETION_MANAGER_H_
