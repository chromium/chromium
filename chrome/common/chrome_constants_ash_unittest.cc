// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"

#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

TEST(ChromeConstants, InitialProfile) {
  // chrome::kInitialProfile must be exactly as same as
  // ash::kSigninBrowserContextBaseName.
  EXPECT_STREQ(chrome::kInitialProfile, ash::kSigninBrowserContextBaseName);
}

}  // namespace chrome
