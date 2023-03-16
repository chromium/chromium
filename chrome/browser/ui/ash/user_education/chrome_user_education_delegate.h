// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_USER_EDUCATION_CHROME_USER_EDUCATION_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_USER_EDUCATION_CHROME_USER_EDUCATION_DELEGATE_H_

#include "ash/user_education/user_education_delegate.h"
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
  void RegisterTutorial(
      const AccountId& account_id,
      user_education::TutorialIdentifier tutorial_id,
      user_education::TutorialDescription tutorial_description) override;
  void StartTutorial(const AccountId& account_id,
                     user_education::TutorialIdentifier tutorial_id,
                     ui::ElementContext element_context,
                     base::OnceClosure completed_callback,
                     base::OnceClosure aborted_callback) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // The profile manager is observed in order to ensure that all necessary
  // tutorial dependencies are registered for the primary user profile.
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_USER_EDUCATION_CHROME_USER_EDUCATION_DELEGATE_H_
