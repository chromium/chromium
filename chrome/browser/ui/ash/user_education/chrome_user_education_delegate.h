// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_USER_EDUCATION_CHROME_USER_EDUCATION_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_USER_EDUCATION_CHROME_USER_EDUCATION_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/user_education/user_education_delegate.h"
#include "ash/user_education/user_education_types.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

class ProfileManager;

// The delegate of the `UserEducationController` which facilitates communication
// between Ash and user education services in the browser.
class ChromeUserEducationDelegate : public ash::UserEducationDelegate,
                                    public ProfileManagerObserver {
 public:
  ChromeUserEducationDelegate();
  ChromeUserEducationDelegate(const ChromeUserEducationDelegate&) = delete;
  ChromeUserEducationDelegate& operator=(const ChromeUserEducationDelegate&) =
      delete;
  ~ChromeUserEducationDelegate() override;

 private:
  // ash::UserEducationDelegate:
  std::optional<ui::ElementIdentifier> GetElementIdentifierForAppId(
      const std::string& app_id) const override;
  const std::optional<bool>& IsNewUser(
      const AccountId& account_id) const override;
  bool IsTutorialRegistered(const AccountId& account_id,
                            ash::TutorialId tutorial_id) const override;
  void RegisterTutorial(
      const AccountId& account_id,
      ash::TutorialId tutorial_id,
      user_education::TutorialDescription tutorial_description) override;
  void StartTutorial(const AccountId& account_id,
                     ash::TutorialId tutorial_id,
                     ui::ElementContext element_context,
                     base::OnceClosure completed_callback,
                     base::OnceClosure aborted_callback) override;
  void AbortTutorial(
      const AccountId& account_id,
      std::optional<ash::TutorialId> tutorial_id = std::nullopt) override;
  void LaunchSystemWebAppAsync(const AccountId& account_id,
                               ash::SystemWebAppType system_web_app_type,
                               apps::LaunchSource launch_source,
                               int64_t display_id) override;
  bool IsRunningTutorial(
      const AccountId& account_id,
      std::optional<ash::TutorialId> tutorial_id = std::nullopt) const override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // If present, indicates whether the user associated with the primary profile
  // is considered new. A user is considered new if the first app list sync in
  // the session was the first sync ever across all ChromeOS devices and
  // sessions for the given user. As such, this value is absent until the first
  // app list sync of the session is completed.
  std::optional<bool> is_primary_profile_new_user_;

  // The profile manager is observed in order to ensure that all necessary
  // tutorial dependencies are registered for the primary user profile.
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::WeakPtrFactory<ChromeUserEducationDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_USER_EDUCATION_CHROME_USER_EDUCATION_DELEGATE_H_
