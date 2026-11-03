// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_error_strings.h"

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/grit/generated_resources.h"
#include "device/fido/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace webauthn {
namespace {

class PasskeyUnlockErrorStringsTest : public testing::Test {
 protected:
  void EnableUiExperimentArm(std::string arm_name) {
    feature_params_[device::kPasskeyUnlockErrorUiExperimentArm.name] = arm_name;
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        device::kPasskeyUnlockErrorUi, feature_params_);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::map<std::string, std::string> feature_params_;
};

TEST_F(PasskeyUnlockErrorStringsTest, TextLablesForDifferentUiExperimentArms) {
  EnableUiExperimentArm("text_with_verify_wording");
  EXPECT_EQ(GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_VERIFY));
  EXPECT_EQ(GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_VERIFY));
  EXPECT_EQ(
      GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_VERIFY));

  EnableUiExperimentArm("text_with_get_wording");
  EXPECT_EQ(GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_GET));
  EXPECT_EQ(GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_GET));
  EXPECT_EQ(
      GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_GET));

  EnableUiExperimentArm("text_with_unlock_wording");
  EXPECT_EQ(GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_UNLOCK));
  EXPECT_EQ(GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_UNLOCK));
  EXPECT_EQ(
      GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_UNLOCK));
}

}  // namespace
}  // namespace webauthn
