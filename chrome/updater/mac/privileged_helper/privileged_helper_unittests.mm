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
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  ASSERT_TRUE(
      VerifyUpdaterSignature(src_dir.Append("third_party")
                                 .Append("updater")
                                 .Append("chrome_mac_universal_prod")
                                 .Append("cipd")
                                 .Append(PRODUCT_FULLNAME_STRING ".app")));
  ASSERT_FALSE(
      VerifyUpdaterSignature(src_dir.Append("third_party")
                                 .Append("updater")
                                 .Append("chrome_mac_universal")
                                 .Append("cipd")
                                 .Append(PRODUCT_FULLNAME_STRING "_test.app")));
}

#endif

}  // namespace updater
