// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/language_packs_util.h"

#include "chromeos/ash/components/language_packs/language_pack_manager.h"
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

// For Handwriting we only keep the language part, not the country/region.
TEST(LanguagePacksUtil, ResolveLocaleHandwriting) {
  EXPECT_EQ(ResolveLocale(kHandwritingFeatureId, "en-US"), "en");
  EXPECT_EQ(ResolveLocale(kHandwritingFeatureId, "en-us"), "en");
  EXPECT_EQ(ResolveLocale(kHandwritingFeatureId, "fr"), "fr");
  EXPECT_EQ(ResolveLocale(kHandwritingFeatureId, "it-IT"), "it");
  EXPECT_EQ(ResolveLocale(kHandwritingFeatureId, "zh"), "zh");
  EXPECT_EQ(ResolveLocale(kHandwritingFeatureId, "zh-TW"), "zh");

  // Chinese HongKong is an exception.
  EXPECT_EQ(ResolveLocale(kHandwritingFeatureId, "zh-HK"), "zh-HK");
}

TEST(LanguagePacksUtil, ResolveLocaleTts) {
  // For these locales we keep the region.
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "en-AU"), "en-au");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "en-au"), "en-au");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "en-GB"), "en-gb");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "en-gb"), "en-gb");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "en-US"), "en-us");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "en-us"), "en-us");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "es-ES"), "es-es");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "es-es"), "es-es");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "es-US"), "es-us");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "es-us"), "es-us");

  // For all other locales we only keep the language.
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "bn-bd"), "bn");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "fil-ph"), "fil");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "it-it"), "it");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "ja-jp"), "ja");
}

}  // namespace ash::language_packs
