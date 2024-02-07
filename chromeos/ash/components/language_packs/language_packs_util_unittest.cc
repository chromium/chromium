// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/language_packs_util.h"

#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::language_packs {

using ::dlcservice::DlcState;
using ::dlcservice::DlcState_State_INSTALLED;
using ::dlcservice::DlcState_State_INSTALLING;
using ::dlcservice::DlcState_State_NOT_INSTALLED;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(LanguagePacksUtil, ConvertDlcState_EmptyInput) {
  DlcState input;
  PackResult output = ConvertDlcStateToPackResult(input);

  // The default value in the input is 'NOT_INSTALLED'.
  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kNotInstalled);
  EXPECT_EQ(output.operation_error, PackResult::ErrorCode::kNone);
}

TEST(LanguagePacksUtil, ConvertDlcState_NotInstalled) {
  DlcState input;
  input.set_state(DlcState_State_NOT_INSTALLED);
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kNotInstalled);
  EXPECT_EQ(output.operation_error, PackResult::ErrorCode::kNone);

  // Even if the path is set (by mistake) in the input, we should not return it.
  input.set_root_path("/var/somepath");
  output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kNotInstalled);
  EXPECT_TRUE(output.path.empty());
}

TEST(LanguagePacksUtil, ConvertDlcState_Installing) {
  DlcState input;
  input.set_state(DlcState_State_INSTALLING);
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kInProgress);
  EXPECT_EQ(output.operation_error, PackResult::ErrorCode::kNone);

  // Even if the path is set (by mistake) in the input, we should not return it.
  input.set_root_path("/var/somepath");
  output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kInProgress);
  EXPECT_TRUE(output.path.empty());
}

TEST(LanguagePacksUtil, ConvertDlcState_Installed) {
  DlcState input;
  input.set_state(DlcState_State_INSTALLED);
  input.set_root_path("/var/somepath");
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kInstalled);
  EXPECT_EQ(output.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_EQ(output.path, "/var/somepath");
}

// Tests the behaviour in case the state received from the input in not a valid
// value. This could happen for example if the proto changes without notice.
TEST(LanguagePacksUtil, ConvertDlcState_MalformedProto) {
  DlcState input;
  // Enum value '3' is beyond currently defined values.
  input.set_state(static_cast<dlcservice::DlcState_State>(3));
  input.set_root_path("/var/somepath");
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kUnknown);
  EXPECT_EQ(output.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_TRUE(output.path.empty());
}

TEST(LanguagePacksUtil, ConvertDlcState_ErrorSet) {
  DlcState input;
  input.set_last_error_code(dlcservice::kErrorNeedReboot);
  PackResult output = ConvertDlcStateToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kNotInstalled);
  EXPECT_EQ(output.operation_error, PackResult::ErrorCode::kNeedReboot);
  EXPECT_TRUE(output.path.empty());
}

TEST(LanguagePacksUtil, ConvertDlcInstallResult_Success) {
  DlcserviceClient::InstallResult input;
  input.error = "";
  input.root_path = "/var/somepath";
  PackResult output = ConvertDlcInstallResultToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kInstalled);
  EXPECT_EQ(output.operation_error, PackResult::ErrorCode::kNone);
  EXPECT_EQ(output.path, "/var/somepath");
}

TEST(LanguagePacksUtil, ConvertDlcInstallResult_Error) {
  DlcserviceClient::InstallResult input;
  input.error = dlcservice::kErrorInternal;
  PackResult output = ConvertDlcInstallResultToPackResult(input);

  EXPECT_EQ(output.pack_state, PackResult::StatusCode::kUnknown);
  EXPECT_EQ(output.operation_error, PackResult::ErrorCode::kOther);
  EXPECT_TRUE(output.path.empty());
}

// Tests the conversion of all error types returned by DlcserviceClient.
TEST(LanguagePacksUtil, ConvertDlcError_AllErrorsTypes) {
  EXPECT_EQ(ConvertDlcErrorToErrorCode(""), PackResult::ErrorCode::kNone);
  EXPECT_EQ(ConvertDlcErrorToErrorCode(dlcservice::kErrorNone),
            PackResult::ErrorCode::kNone);
  EXPECT_EQ(ConvertDlcErrorToErrorCode(dlcservice::kErrorAllocation),
            PackResult::ErrorCode::kAllocation);
  EXPECT_EQ(ConvertDlcErrorToErrorCode(dlcservice::kErrorInvalidDlc),
            PackResult::ErrorCode::kWrongId);
  EXPECT_EQ(ConvertDlcErrorToErrorCode(dlcservice::kErrorNeedReboot),
            PackResult::ErrorCode::kNeedReboot);
  EXPECT_EQ(ConvertDlcErrorToErrorCode(dlcservice::kErrorNoImageFound),
            PackResult::ErrorCode::kOther);
  EXPECT_EQ(ConvertDlcErrorToErrorCode(dlcservice::kErrorInternal),
            PackResult::ErrorCode::kOther);
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
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "pt-BR"), "pt-br");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "pt-br"), "pt-br");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "pt-PT"), "pt-pt");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "pt-pt"), "pt-pt");

  // For all other locales we only keep the language.
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "bn-bd"), "bn");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "fil-ph"), "fil");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "it-it"), "it");
  EXPECT_EQ(ResolveLocale(kTtsFeatureId, "ja-jp"), "ja");
}

TEST(LanguagePacksUtil, ResolveLocaleFonts) {
  // Language pack resolution is handled by the client.
  EXPECT_EQ(ResolveLocale(kFontsFeatureId, "ja"), "ja");
  EXPECT_EQ(ResolveLocale(kFontsFeatureId, "ko"), "ko");
}

TEST(LanguagePacksUtil, MapThenFilterStringsNoInput) {
  EXPECT_THAT(MapThenFilterStrings(
                  {}, base::BindRepeating(
                          [](const std::string&) -> std::optional<std::string> {
                            return "ignored";
                          })),
              IsEmpty());
}

TEST(LanguagePacksUtil, MapThenFilterStringsAllToNullopt) {
  EXPECT_THAT(MapThenFilterStrings(
                  {{"en", "de"}},
                  base::BindRepeating(
                      [](const std::string&) -> std::optional<std::string> {
                        return std::nullopt;
                      })),
              IsEmpty());
}

TEST(LanguagePacksUtil, MapThenFilterStringsAllToUniqueStrings) {
  EXPECT_THAT(
      MapThenFilterStrings(
          {{"en", "de"}},
          base::BindRepeating(
              [](const std::string& input) -> std::optional<std::string> {
                return input;
              })),
      UnorderedElementsAre("en", "de"));
}

TEST(LanguagePacksUtil, MapThenFilterStringsRepeatedString) {
  EXPECT_THAT(
      MapThenFilterStrings(
          {{"repeat", "unique", "repeat"}},
          base::BindRepeating(
              [](const std::string& input) -> std::optional<std::string> {
                return input;
              })),
      UnorderedElementsAre("repeat", "unique"));
}

TEST(LanguagePacksUtil, MapThenFilterStringsSomeNullopt) {
  EXPECT_THAT(
      MapThenFilterStrings(
          {{"pass_1", "fail", "pass_2"}},
          base::BindRepeating(
              [](const std::string& input) -> std::optional<std::string> {
                return (input == "fail") ? std::nullopt
                                         : std::optional<std::string>(input);
              })),
      UnorderedElementsAre("pass_1", "pass_2"));
}

TEST(LanguagePacksUtil, MapThenFilterStringsDeduplicateOutput) {
  EXPECT_THAT(
      MapThenFilterStrings(
          {{"a", "dedup-1", "dedup-2"}},
          base::BindRepeating(
              [](const std::string& input) -> std::optional<std::string> {
                return (input.length() < 2) ? input : "dedup";
              })),
      UnorderedElementsAre("a", "dedup"));
}

TEST(LanguagePacksUtil, MapThenFilterStringsDisjointSet) {
  EXPECT_THAT(
      MapThenFilterStrings(
          {{"a", "b", "d"}},
          base::BindRepeating(
              [](const std::string& input) -> std::optional<std::string> {
                return "something else";
              })),
      UnorderedElementsAre("something else"));
}

TEST(LanguagePacksUtil, ExtractInputMethodsFromPrefsEmpty) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  pref->registry()->RegisterStringPref(prefs::kLanguagePreloadEngines,
                                       std::string());

  EXPECT_THAT(ExtractInputMethodsFromPrefs(pref.get()), IsEmpty());
}

TEST(LanguagePacksUtil, ExtractInputMethodsFromPrefsOne) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  pref->registry()->RegisterStringPref(prefs::kLanguagePreloadEngines,
                                       "xkb:it::ita");

  EXPECT_THAT(ExtractInputMethodsFromPrefs(pref.get()),
              UnorderedElementsAre("xkb:it::ita"));
}

TEST(LanguagePacksUtil, ExtractInputMethodsFromPrefsMultiple) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  pref->registry()->RegisterStringPref(prefs::kLanguagePreloadEngines,
                                       "xkb:it::ita,xkb:fr::fra,xkb:de::ger");

  EXPECT_THAT(
      ExtractInputMethodsFromPrefs(pref.get()),
      UnorderedElementsAre("xkb:it::ita", "xkb:fr::fra", "xkb:de::ger"));
}

}  // namespace ash::language_packs
