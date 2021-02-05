// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/profile_test_helper.h"

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#endif

std::string TestProfileTypeToString(
    const ::testing::TestParamInfo<TestProfileType>& info) {
  switch (info.param) {
    case TestProfileType::kRegular:
      return "Regular";
    case TestProfileType::kIncognito:
      return "Incognito";
    case TestProfileType::kGuest:
      return "Guest";
  }
}

void ConfigureCommandLineForGuestMode(base::CommandLine* command_line) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  command_line->AppendSwitch(chromeos::switches::kGuestSession);
  command_line->AppendSwitch(::switches::kIncognito);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "hash");
  command_line->AppendSwitchASCII(
      chromeos::switches::kLoginUser,
      user_manager::GuestAccountId().GetUserEmail());
#else
  NOTREACHED();
#endif
}
