// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FIRST_RUN_TEST_UTIL_H_
#define CHROME_BROWSER_UI_STARTUP_FIRST_RUN_TEST_UTIL_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/test/base/in_process_browser_test.h"

class Profile;
class FirstRunService;

// Updates command line flags to make the test believe that we are on a fresh
// install. Intended to be called from the test body. Note that if a sentinel
// file exists (e.g. a PRE_Test ran) this method might have no effect.
void SetIsFirstRun(bool is_first_run);

// Returns the value of the `prefs::kFirstRunFinished` local pref.
// Causes the test to fail if local prefs are not available. In unit tests, make
// sure to configure them, e.g. by using `ScopedTestingLocalState`.
bool GetFirstRunFinishedPrefValue();

// Helps with testing the behaviour of the `FirstRunService`.
//
// In the test body, a browser window is opened, the process is marked as being
// in the first run (per `first_run::IsChromeFirstRun()`) and the
// `FirstRunService` does not exist yet.
class FirstRunServiceBrowserTestBase : public InProcessBrowserTest {
 public:
  FirstRunServiceBrowserTestBase();
  FirstRunServiceBrowserTestBase(const FirstRunServiceBrowserTestBase&) =
      delete;
  FirstRunServiceBrowserTestBase& operator=(
      const FirstRunServiceBrowserTestBase&) = delete;
  ~FirstRunServiceBrowserTestBase() override;

  void SetUpOnMainThread() override;

 protected:
  Profile* profile() const;
  FirstRunService* fre_service() const;

  // -- Utilities for checks on the profile state after the first run ----------

  std::u16string GetProfileName() const;
  bool IsUsingDefaultProfileName() const;

  // Returns whether the profile's name is the default one (e.g. "Person 1").
  // Also asserts that the name is marked as default (`IsUsingDefaultName`)
  // according to the pattern of the actual name.
  bool IsProfileNameDefault() const;

 private:
  ProfileNameResolver::ScopedInfoFetchTimeoutOverride
      profile_name_fetch_timeout_override_ =
          ProfileNameResolver::CreateScopedInfoFetchTimeoutOverrideForTesting(
              base::TimeDelta());
};

#endif  // CHROME_BROWSER_UI_STARTUP_FIRST_RUN_TEST_UTIL_H_
