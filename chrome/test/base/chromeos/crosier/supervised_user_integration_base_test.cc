// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/supervised_user_integration_base_test.h"

#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

SupervisedUserIntegrationBaseTest::SupervisedUserIntegrationBaseTest() {
  set_exit_when_last_browser_closes(false);

  // Allows network access for production Gaia.
  SetAllowNetworkAccessToHostResolutions();

  login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kCustomGaiaLogin);
  login_mixin().set_custom_gaia_login_delegate(&delegate_);
}

SupervisedUserIntegrationBaseTest::~SupervisedUserIntegrationBaseTest() =
    default;

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
