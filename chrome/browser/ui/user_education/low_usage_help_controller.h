// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_LOW_USAGE_HELP_CONTROLLER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_LOW_USAGE_HELP_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"

// Class that provides additional help to users who don't use Chrome often.
// There is one controller per profile, and the promo will only show in the
// active browser.
class LowUsageHelpController : public base::SupportsUserData::Data {
 public:
  ~LowUsageHelpController() override;

  // Creates or returns the existing controller for a given `profile`.
  static LowUsageHelpController* MaybeCreateForProfile(Profile* profile);

  // Returns the controller already created for a given `profile`, or null if
  // none exists.
  static LowUsageHelpController* GetForProfileForTesting(Profile* profile);

 private:
  explicit LowUsageHelpController(Profile* profile);

  void OnLowUsageSession();
  void MaybeShowPromo();

  // Sometimes during startup there's no active browser window yet; retry after
  // a short delay.
  bool retrying_ = false;
  base::OneShotTimer retry_timer_;

  const raw_ptr<Profile> profile_;
  base::CallbackListSubscription subscription_;
  base::WeakPtrFactory<LowUsageHelpController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_LOW_USAGE_HELP_CONTROLLER_H_
