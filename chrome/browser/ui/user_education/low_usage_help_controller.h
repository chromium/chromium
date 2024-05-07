// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_LOW_USAGE_HELP_CONTROLLER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_LOW_USAGE_HELP_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/browser.h"

// Class that provides additional help to users who don't use Chrome often.
class LowUsageHelpController : public base::SupportsUserData::Data {
 public:
  ~LowUsageHelpController() override;

  // Creates or returns the existing controller for a given Browser.
  static LowUsageHelpController* MaybeCreateForBrowser(Browser* browser);

  // Returns the controller already created for a given browser, or null if
  // none exists.
  static LowUsageHelpController* GetForBrowserForTesting(Browser* browser);

 private:
  explicit LowUsageHelpController(Browser* browser);

  void OnLowUsageSession();
  void MaybeShowPromo();

  const raw_ptr<Browser> browser_;
  base::CallbackListSubscription subscription_;
  base::WeakPtrFactory<LowUsageHelpController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_LOW_USAGE_HELP_CONTROLLER_H_
