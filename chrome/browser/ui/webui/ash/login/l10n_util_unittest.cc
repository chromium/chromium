// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/l10n_util.h"

#include <stddef.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util_test_util.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/mock_component_extension_ime_manager_delegate.h"

namespace ash {

namespace {

void VerifyOnlyUILanguages(const base::Value::List& list) {
  for (const auto& value : list) {
    ASSERT_TRUE(value.is_dict());
    const base::Value::Dict& dict = value.GetDict();
    const std::string* code = dict.FindString("code");
    ASSERT_TRUE(code);
    EXPECT_NE("ga", *code)
        << "Irish is an example language which has input method "
        << "but can't use it as UI language.";
  }
}

void VerifyLanguageCode(const base::Value::List& list,
                        size_t index,
                        const std::string& expected_code) {
  const base::Value::Dict& value = list[index].GetDict();
  const std::string* actual_code = value.FindString("code");
  ASSERT_TRUE(actual_code);
  EXPECT_EQ(expected_code, *actual_code)
      << "Wrong language code at index " << index << ".";
}

}  // namespace

class L10nUtilTest : public testing::Test {
 public:
  L10nUtilTest();

  L10nUtilTest(const L10nUtilTest&) = delete;
  L10nUtilTest& operator=(const L10nUtilTest&) = delete;

  ~L10nUtilTest() override = default;

  void SetInputMethods1();
  void SetInputMethods2();

 protected:
  MockInputMethodManagerWithInputMethods input_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  system::ScopedFakeStatisticsProvider scoped_fake_statistics_provider_;
};

L10nUtilTest::L10nUtilTest() {
  auto mock_component_extension_ime_manager_delegate = std::make_unique<
      input_method::MockComponentExtensionIMEManagerDelegate>();
  input_manager_.SetComponentExtensionIMEManager(
      std::make_unique<ComponentExtensionIMEManager>(
          std::move(mock_component_extension_ime_manager_delegate)));

  base::RunLoop().RunUntilIdle();
}

void L10nUtilTest::SetInputMethods1() {
  input_manager_.AddInputMethod("xkb:us::eng", "us", "en-US");
  input_manager_.AddInputMethod("xkb:fr::fra", "fr", "fr");
  input_manager_.AddInputMethod("xkb:be::fra", "be", "fr");
  input_manager_.AddInputMethod("xkb:ie::ga", "ga", "ga");
}

void L10nUtilTest::SetInputMethods2() {
  input_manager_.AddInputMethod("xkb:us::eng", "us", "en-US");
  input_manager_.AddInputMethod("xkb:ch:fr:fra", "ch(fr)", "fr");
  input_manager_.AddInputMethod("xkb:ch::ger", "ch", "de");
  input_manager_.AddInputMethod("xkb:it::ita", "it", "it");
  input_manager_.AddInputMethod("xkb:ie::ga", "ga", "ga");
}

TEST_F(L10nUtilTest, GetUILanguageList) {
  SetInputMethods1();

  // This requires initialized StatisticsProvider (see L10nUtilTest()).
  auto list(GetUILanguageList(nullptr, std::string(), &input_manager_));

  VerifyOnlyUILanguages(list);
}

TEST_F(L10nUtilTest, FindMostRelevantLocale) {
  base::Value::List available_locales;
  for (const char* locale : {"de", "fr", "en-GB"}) {
    available_locales.Append(base::Value::Dict().Set("value", locale));
  }

  std::vector<std::string> most_relevant_language_codes;
  EXPECT_EQ("en-US", FindMostRelevantLocale(most_relevant_language_codes,
                                            available_locales,
                                            "en-US"));

  most_relevant_language_codes.push_back("xx");
  EXPECT_EQ("en-US", FindMostRelevantLocale(most_relevant_language_codes,
                                            available_locales,
                                            "en-US"));

  most_relevant_language_codes.push_back("fr");
  EXPECT_EQ("fr", FindMostRelevantLocale(most_relevant_language_codes,
                                         available_locales,
                                         "en-US"));

  most_relevant_language_codes.push_back("de");
  EXPECT_EQ("fr", FindMostRelevantLocale(most_relevant_language_codes,
                                         available_locales,
                                         "en-US"));
}

void InitStartupCustomizationDocumentForTesting(const std::string& manifest) {
  StartupCustomizationDocument::GetInstance()->LoadManifestFromString(manifest);
  StartupCustomizationDocument::GetInstance()->Init(
      system::StatisticsProvider::GetInstance());
}

const char kStartupManifest[] =
    "{\n"
    "  \"version\": \"1.0\",\n"
    "  \"initial_locale\" : \"fr,en-US,de,ga,it\",\n"
    "  \"initial_timezone\" : \"Europe/Zurich\",\n"
    "  \"keyboard_layout\" : \"xkb:ch:fr:fra\",\n"
    "  \"registration_url\" : \"http://www.google.com\",\n"
    "  \"setup_content\" : {\n"
    "    \"default\" : {\n"
    "      \"help_page\" : \"file:///opt/oem/help/en-US/help.html\",\n"
    "      \"eula_page\" : \"file:///opt/oem/eula/en-US/eula.html\",\n"
    "    },\n"
    "  },"
    "}";

TEST_F(L10nUtilTest, GetUILanguageListMulti) {
  InitStartupCustomizationDocumentForTesting(kStartupManifest);
  SetInputMethods2();

  // This requires initialized StatisticsProvider (see L10nUtilTest()).
  auto list(GetUILanguageList(nullptr, std::string(), &input_manager_));

  VerifyOnlyUILanguages(list);

  // (4 languages (except Irish) + divider) = 5 + all other languages
  ASSERT_LE(5u, list.size());

  VerifyLanguageCode(list, 0, "fr");
  VerifyLanguageCode(list, 1, "en-US");
  VerifyLanguageCode(list, 2, "de");
  VerifyLanguageCode(list, 3, "it");
  VerifyLanguageCode(list, 4, kMostRelevantLanguagesDivider);
}

TEST_F(L10nUtilTest, GetUILanguageListWithMostRelevant) {
  std::vector<std::string> most_relevant_language_codes;
  most_relevant_language_codes.push_back("it");
  most_relevant_language_codes.push_back("de");
  most_relevant_language_codes.push_back("nonexistent");

  // This requires initialized StatisticsProvider (see L10nUtilTest()).
  auto list(GetUILanguageList(&most_relevant_language_codes, std::string(),
                              &input_manager_));

  VerifyOnlyUILanguages(list);

  ASSERT_LE(3u, list.size());

  VerifyLanguageCode(list, 0, "it");
  VerifyLanguageCode(list, 1, "de");
  VerifyLanguageCode(list, 2, kMostRelevantLanguagesDivider);
}

}  // namespace ash
