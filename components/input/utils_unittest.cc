// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/utils.h"

#include "base/android/android_info.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/input/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace input {

TEST(UtilsTest, InputToVizNotSupportedOnOlderSecurityPatchLevel) {
  const std::vector<std::pair<std::string, bool>> security_patches = {
      {"2024-03-06", false}, {"2025-01-06", false}, {"2025-02-01", false},
      {"2025-02-05", true},  {"2025-03-01", true},  {"2025-12-31", true},
  };

  for (const auto& [date, expectation] : security_patches) {
    EXPECT_EQ(InputUtils::HasSecurityUpdate(
                  date, base::android::android_info::SdkVersion::SDK_VERSION_V),
              expectation);
  }
}

TEST(UtilsTest, AndroidBaklavaPlusHasSecurityPatch) {
  const std::vector<std::string> security_patches = {
      "2024-03-06", "2025-01-06", "2025-02-01",
      "2025-02-05", "2025-03-01", "2025-12-31",
  };

  for (const auto& date : security_patches) {
    EXPECT_TRUE(InputUtils::HasSecurityUpdate(
        date, base::android::android_info::SdkVersion::SDK_VERSION_BAKLAVA));
  }
}

}  // namespace input
