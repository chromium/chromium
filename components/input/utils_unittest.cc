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

class UtilsTest : public testing::Test {
 public:
  UtilsTest() {
    scoped_feature_list_.InitAndEnableFeature(input::features::kInputOnViz);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UtilsTest, InputToVizNotSupportedOnOlderSecurityPatchLevel) {
  const std::vector<std::pair<std::string, bool>> security_patches = {
      {"2024-03-06", false}, {"2025-01-06", false}, {"2025-02-01", false},
      {"2025-02-05", true},  {"2025-03-01", true},  {"2025-12-31", true},
  };

  for (const auto& [date, expectation] : security_patches) {
    base::android::android_info::AndroidInfo android_info(
        /* device= */ base::EmptyString(),
        /* manufacturer= */ base::EmptyString(),
        /* model= */ base::EmptyString(),
        /* brand= */ base::EmptyString(),
        /* android_build_id= */ base::EmptyString(),
        /* build_type= */ base::EmptyString(),
        /* board= */ base::EmptyString(),
        /* android_build_fp= */ base::EmptyString(),
        /* sdk_int= */ base::android::android_info::SdkVersion::SDK_VERSION_V,
        /* is_debug_android= */ false,
        /* version_incremental= */ base::EmptyString(),
        /* hardware= */ base::EmptyString(),
        /* codename= */ base::EmptyString(),
        /* soc_manufacturer= */ base::EmptyString(),
        /* abi_name= */ base::EmptyString(),
        /* security_patch= */ date.data());
    base::android::android_info::SetAndroidInfoForTesting(android_info);
    InputUtils::ResetInitializedForTesting();
    EXPECT_EQ(InputUtils::IsTransferInputToVizSupported(), expectation);
  }
}

}  // namespace input
