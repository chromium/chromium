// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/branded_constants.h"

#include <string>

#include "base/strings/string_util.h"
#include "chrome/updater/updater_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(BrandedConstantsTest, SetupMutex) {
  EXPECT_TRUE(base::EndsWith(kSetupMutex, kUpdaterVersion,
                             base::CompareCase::INSENSITIVE_ASCII));
}

}  // namespace updater
