// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/updater/mac/privileged_helper/service.h"
#include "chrome/updater/updater_branding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST(PrivilegedHelperTest, VerifyUpdaterSignature) {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  ASSERT_TRUE(
      VerifyUpdaterSignature(exe_path.Append("old_updater")
                                 .Append("chrome_mac_universal_prod")
                                 .Append(PRODUCT_FULLNAME_STRING ".app")));
  ASSERT_FALSE(
      VerifyUpdaterSignature(exe_path.Append("old_updater")
                                 .Append("chrome_mac_universal")
                                 .Append(PRODUCT_FULLNAME_STRING ".app")));
}

#endif

}  // namespace updater
