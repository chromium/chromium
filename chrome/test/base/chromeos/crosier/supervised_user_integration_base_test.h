// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_SUPERVISED_USER_INTEGRATION_BASE_TEST_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_SUPERVISED_USER_INTEGRATION_BASE_TEST_H_

#include "build/branding_buildflags.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/supervised_user_login_delegate.h"

// Tests using production GAIA can only run on branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Base test class that sets up Crosier production GAIA login fixture for
// supervised users.
class SupervisedUserIntegrationBaseTest : public AshIntegrationTest {
 public:
  SupervisedUserIntegrationBaseTest();
  SupervisedUserIntegrationBaseTest(const SupervisedUserIntegrationBaseTest&) =
      delete;
  SupervisedUserIntegrationBaseTest& operator=(
      const SupervisedUserIntegrationBaseTest&) = delete;
  ~SupervisedUserIntegrationBaseTest() override;

 protected:
  SupervisedUserLoginDelegate delegate_;
};

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_SUPERVISED_USER_INTEGRATION_BASE_TEST_H_
