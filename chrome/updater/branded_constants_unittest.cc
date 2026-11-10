// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/branded_constants.h"

#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chrome/updater/updater_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

bool IsBracedGuid(std::string_view id) {
  return id.size() == 38 && id.front() == '{' && id.back() == '}' &&
         base::Uuid::ParseCaseInsensitive(id.substr(1, 36)).is_valid();
}

}  // namespace

TEST(BrandedConstantsTest, SetupMutex) {
  EXPECT_TRUE(base::EndsWith(kSetupMutex, kUpdaterVersion,
                             base::CompareCase::INSENSITIVE_ASCII));
}

TEST(BrandedConstantsTest, AppIdsAreSubstitutedGuids) {
  EXPECT_TRUE(IsBracedGuid(kUpdaterAppId));
  EXPECT_TRUE(IsBracedGuid(kQualificationAppId));
  EXPECT_TRUE(IsBracedGuid(kPlatformExperienceHelperAppId));
}

}  // namespace updater
