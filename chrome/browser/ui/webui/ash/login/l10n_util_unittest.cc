// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/l10n_util.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/ime/ash/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

class TestInputMethodManager : public MockInputMethodManagerWithInputMethods {
 public:
  TestInputMethodManager() : util_(&delegate_) {
    delegate_.set_get_localized_string_callback(
        base::BindRepeating(&l10n_util::GetStringUTF16));
  }

  input_method::InputMethodUtil* GetInputMethodUtil() override {
    return &util_;
  }

 private:
  input_method::FakeInputMethodDelegate delegate_;
  input_method::InputMethodUtil util_;
};

void VerifyOnlyUILanguages(const base::ListValue& list) {
  for (const auto& value : list) {
    ASSERT_TRUE(value.is_dict());
    const base::DictValue& dict = value.GetDict();
    const std::string* code = dict.FindString("code");
    ASSERT_TRUE(code);
    EXPECT_NE("ga", *code)
        << "Irish is an example language which has input method "
        << "but can't use it as UI language.";
  }
}

void VerifyLanguageCode(const base::ListValue& list,
                        size_t index,
                        const std::string& expected_code) {
  const base::DictValue& value = list[index].GetDict();
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

  input_method::InputMethodDescriptors GetDescriptors() {
    return CHECK_DEREF(input_manager_.GetComponentExtensionIMEManager())
        .GetXkbIMEAsInputMethodDescriptor();
  }

 protected:
  TestInputMethodManager input_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  system::ScopedFakeStatisticsProvider scoped_fake_statistics_provider_;
};

L10nUtilTest::L10nUtilTest() {
  auto mock_component_extension_ime_manager_delegate = std::make_unique<
      input_method::MockComponentExtensionIMEManagerDelegate>();
  input_manager_.SetComponentExtensionIMEManager(
      std::make_unique<ComponentExtensionIMEManager>(
          std::move(mock_component_extension_ime_manager_delegate)));
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
  auto list(GetUILanguageList(
      TestingBrowserProcess::GetGlobal()->GetApplicationLocale(), nullptr,
      std::string(), GetDescriptors()));

  VerifyOnlyUILanguages(list);
}

TEST_F(L10nUtilTest, ResolveUILanguageList) {
  SetInputMethods1();

  base::test::TestFuture<base::ListValue, const std::string&,
                         const std::string&>
      future;
  ResolveUILanguageList(TestingBrowserProcess::GetGlobal()
                            ->GetFeatures()
                            ->application_locale_storage()
                            ->GetTag(),
                        /*language_switch_result=*/nullptr, input_manager_,
                        future.GetCallback());

  auto [language_list, language_list_locale, selected_language] = future.Take();
  VerifyOnlyUILanguages(language_list);
  EXPECT_FALSE(language_list.empty());
  EXPECT_EQ(language_list_locale,
            TestingBrowserProcess::GetGlobal()->GetApplicationLocale());
  EXPECT_EQ(selected_language,
            TestingBrowserProcess::GetGlobal()->GetApplicationLocale());
}

TEST_F(L10nUtilTest, ResolveUILanguageListWithSwitchResult) {
  SetInputMethods1();

  base::test::TestFuture<base::ListValue, const std::string&,
                         const std::string&>
      future;
  ResolveUILanguageList(
      TestingBrowserProcess::GetGlobal()
          ->GetFeatures()
          ->application_locale_storage()
          ->GetTag(),
      std::make_unique<locale_util::LanguageSwitchResult>("fr", "fr", true),
      input_manager_, future.GetCallback());

  auto [language_list, language_list_locale, selected_language] = future.Take();
  VerifyOnlyUILanguages(language_list);
  EXPECT_FALSE(language_list.empty());
  EXPECT_EQ(language_list_locale, "fr");
  EXPECT_EQ(selected_language, "fr");
}

TEST_F(L10nUtilTest, FindMostRelevantLocale) {
  base::ListValue available_locales;
  for (const char* locale : {"de", "fr", "en-GB"}) {
    available_locales.Append(base::DictValue().Set("value", locale));
  }

  std::vector<std::string> most_relevant_language_codes;
  EXPECT_EQ("en-US", FindMostRelevantLocale(most_relevant_language_codes,
                                            available_locales, "en-US"));

  most_relevant_language_codes.push_back("xx");
  EXPECT_EQ("en-US", FindMostRelevantLocale(most_relevant_language_codes,
                                            available_locales, "en-US"));

  most_relevant_language_codes.push_back("fr");
  EXPECT_EQ("fr", FindMostRelevantLocale(most_relevant_language_codes,
                                         available_locales, "en-US"));

  most_relevant_language_codes.push_back("de");
  EXPECT_EQ("fr", FindMostRelevantLocale(most_relevant_language_codes,
                                         available_locales, "en-US"));
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
  auto list(GetUILanguageList(
      TestingBrowserProcess::GetGlobal()->GetApplicationLocale(), nullptr,
      std::string(), GetDescriptors()));

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
  auto list(GetUILanguageList(
      TestingBrowserProcess::GetGlobal()->GetApplicationLocale(),
      &most_relevant_language_codes, std::string(), GetDescriptors()));

  VerifyOnlyUILanguages(list);

  ASSERT_LE(3u, list.size());

  VerifyLanguageCode(list, 0, "it");
  VerifyLanguageCode(list, 1, "de");
  VerifyLanguageCode(list, 2, kMostRelevantLanguagesDivider);
}

TEST_F(L10nUtilTest, GetKeyboardLayoutsForLocaleSelectsLocaleLayout) {
  const std::string us_ime_id =
      input_method::InputMethodUtil::GetFallbackInputMethodDescriptor().id();
  const std::string fr_ime_id =
      extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra");
  input_manager_.GetInputMethodUtil()->AppendInputMethods(
      {input_method::InputMethodDescriptor(
          fr_ime_id, /*name=*/"", /*indicator=*/"FR", /*keyboard_layout=*/"fr",
          /*language_codes=*/{"fr"}, /*is_login_keyboard=*/true,
          /*options_page_url=*/GURL(), /*input_view_url=*/GURL(),
          /*handwriting_language=*/std::nullopt)});

  base::test::TestFuture<base::ListValue> future;
  GetKeyboardLayoutsForLocale(future.GetCallback(), "fr", &input_manager_);

  const base::ListValue& layouts = future.Get();
  ASSERT_EQ(2u, layouts.size());
  EXPECT_EQ(us_ime_id, *layouts[0].GetDict().FindString("value"));
  EXPECT_FALSE(layouts[0].GetDict().FindBool("selected").value_or(true));
  EXPECT_EQ(fr_ime_id, *layouts[1].GetDict().FindString("value"));
  EXPECT_TRUE(layouts[1].GetDict().FindBool("selected").value_or(false));
}

TEST_F(L10nUtilTest,
       GetKeyboardLayoutsForLocaleSelectsFallbackWhenNoXkbLayoutForLocale) {
  const std::string de_ime_id =
      extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger");
  const std::string pinyin_ime_id =
      extension_ime_util::GetInputMethodIDByEngineID("zh-t-i0-pinyin");
  input_method::InputMethodUtil* util = input_manager_.GetInputMethodUtil();
  util->ResetInputMethods(
      {input_method::InputMethodDescriptor(
           de_ime_id, /*name=*/"", /*indicator=*/"DE", /*keyboard_layout=*/"de",
           /*language_codes=*/{"de"}, /*is_login_keyboard=*/true,
           /*options_page_url=*/GURL(), /*input_view_url=*/GURL(),
           /*handwriting_language=*/std::nullopt),
       input_method::InputMethodDescriptor(
           pinyin_ime_id, /*name=*/"Pinyin", /*indicator=*/"CN",
           /*keyboard_layout=*/"us", /*language_codes=*/{"zh-CN"},
           /*is_login_keyboard=*/false, /*options_page_url=*/GURL(),
           /*input_view_url=*/GURL(),
           /*handwriting_language=*/std::nullopt)});
  util->SetHardwareKeyboardLayoutForTesting("xkb:de::ger");

  base::test::TestFuture<base::ListValue> future;
  GetKeyboardLayoutsForLocale(future.GetCallback(), "zh-CN", &input_manager_);

  const base::ListValue& layouts = future.Get();
  ASSERT_EQ(1u, layouts.size());
  EXPECT_EQ(de_ime_id, *layouts[0].GetDict().FindString("value"));
  EXPECT_TRUE(layouts[0].GetDict().FindBool("selected").value_or(false));
}

class L10nUtilTestWithFjordOobe : public L10nUtilTest {
 public:
  void SetUp() override {
    L10nUtilTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kFjordOobeForceEnabled);
  }

  void VerifyAllowlistedLanguages(const base::ListValue& list) {
    const auto& allowlisted_languages =
        fjord_util::GetAllowlistedLanguagesForTesting();
    ASSERT_EQ(allowlisted_languages.size(), list.size());

    for (const base::Value& language : list) {
      const base::DictValue& value = language.GetDict();
      const std::string* language_code = value.FindString("code");
      ASSERT_TRUE(allowlisted_languages.contains(*language_code));
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(L10nUtilTestWithFjordOobe, TestLanguagesFiltered) {
  SetInputMethods1();

  // This requires initialized StatisticsProvider (see L10nUtilTest()).
  auto list(GetUILanguageList(
      TestingBrowserProcess::GetGlobal()->GetApplicationLocale(), nullptr,
      std::string(), GetDescriptors()));

  VerifyAllowlistedLanguages(list);
}

}  // namespace ash
