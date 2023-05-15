// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language/language_packs/language_packs_util.h"

#include "chromeos/ash/components/language/language_packs/language_pack_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::language_packs {

TEST(LanguagePacksUtil, ConvertDlcState_EmptyInput) {
  dlcservice::DlcState input;
  PackResult output = ConvertDlcStateToPackResult(input);

  // The default value in the input is 'NOT_INSTALLED'.
  EXPECT_EQ(output.pack_state, PackResult::NOT_INSTALLED);
}

TEST(LanguagePacksUtil, ConvertDlcState_NotInstalled) {
  dlcservice::DlcState input;
  input.set_state(dlcservice::DlcState_State_NOT_INSTALLED);
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::NOT_INSTALLED);

  // Even if the path is set (by mistake) in the input, we should not return it.
  input.set_root_path("/var/somepath");
  output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::NOT_INSTALLED);
  EXPECT_TRUE(output.path.empty());
}

TEST(LanguagePacksUtil, ConvertDlcState_Installing) {
  dlcservice::DlcState input;
  input.set_state(dlcservice::DlcState_State_INSTALLING);
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::IN_PROGRESS);

  // Even if the path is set (by mistake) in the input, we should not return it.
  input.set_root_path("/var/somepath");
  output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::IN_PROGRESS);
  EXPECT_TRUE(output.path.empty());
}

TEST(LanguagePacksUtil, ConvertDlcState_Installed) {
  dlcservice::DlcState input;
  input.set_state(dlcservice::DlcState_State_INSTALLED);
  input.set_root_path("/var/somepath");
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::INSTALLED);
  EXPECT_EQ(output.path, "/var/somepath");
}

// Tests the behaviour in case the state received from the input in not a valid
// value. This could happen for example if the proto changes without notice.
TEST(LanguagePacksUtil, ConvertDlcState_MalformedProto) {
  dlcservice::DlcState input;
  // Enum value '3' is beyond currently defined values.
  input.set_state(static_cast<dlcservice::DlcState_State>(3));
  input.set_root_path("/var/somepath");
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::UNKNOWN);
  EXPECT_TRUE(output.path.empty());
}

}  // namespace ash::language_packs
