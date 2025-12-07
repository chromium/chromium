// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/translation_view_wrapper_impl.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/boca/babelorca/caption_bubble_settings_impl.h"
#include "chromeos/ash/components/boca/babelorca/pref_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view.h"

namespace ash::babelorca {
namespace {

const std::string kEnglishLanguage = "en-US";
const std::string kFrenchLanguage = "fr";

class TranslationViewWrapperImplBaseTest
    : public captions::TranslationViewWrapperBase::Delegate {
 protected:
  TranslationViewWrapperImplBaseTest()
      : caption_bubble_settings_(InitPrefService(),
                                 kEnglishLanguage,
                                 base::DoNothing()) {}

  void OnLanguageChanged(const std::string& display_language) override {}
  void UpdateLanguageDirection(const std::string& display_language) override {}
  void CaptionSettingsButtonPressed() override {}

  void VerifyTranslationItemsVisible(
      TranslationViewWrapperImpl* translation_view_wrapper,
      bool source_language_visible,
      bool target_language_visible) {
    EXPECT_EQ(translation_view_wrapper->GetSourceLanguageButtonForTesting()
                  ->GetVisible(),
              source_language_visible);
    EXPECT_EQ(translation_view_wrapper->GetTranslateIconAndTextForTesting()
                  ->GetVisible(),
              source_language_visible);
    EXPECT_EQ(translation_view_wrapper->GetTranslateArrowIconForTesting()
                  ->GetVisible(),
              source_language_visible);
    EXPECT_EQ(translation_view_wrapper->GetTargetLanguageButtonForTesting()
                  ->GetVisible(),
              target_language_visible);
  }

  TestingPrefServiceSimple* InitPrefService() {
    pref_service_.registry()->RegisterBooleanPref(prefs::kCaptionBubbleExpanded,
                                                  false);
    pref_service_.registry()->RegisterStringPref(
        prefs::kTranslateTargetLanguageCode, kEnglishLanguage);
    return &pref_service_;
  }

  views::LayoutProvider layout_provider_;
  TestingPrefServiceSimple pref_service_;
  CaptionBubbleSettingsImpl caption_bubble_settings_;
};

struct TranslationViewWrapperImplTestCase {
  std::string test_name;
  bool translate_allowed;
  bool translate_enabled;
  std::string target_language_code = kFrenchLanguage;
  int toggle_text_id = IDS_BOCA_CAPTIONS_TRANSLATION_AVAILABLE_BUTTON_TEXT;
};

class TranslationViewWrapperImplTest
    : public TranslationViewWrapperImplBaseTest,
      public testing::TestWithParam<TranslationViewWrapperImplTestCase> {
 protected:
  TranslationViewWrapperImplTest()
      : translation_view_wrapper_(&caption_bubble_settings_) {}

  views::View translation_container_;
  TranslationViewWrapperImpl translation_view_wrapper_;
};

TEST_P(TranslationViewWrapperImplTest, Init) {
  caption_bubble_settings_.SetTranslateAllowed(GetParam().translate_allowed);
  caption_bubble_settings_.SetLiveTranslateEnabled(
      GetParam().translate_enabled);
  caption_bubble_settings_.SetLiveTranslateTargetLanguageCode(
      GetParam().target_language_code);

  translation_view_wrapper_.Init(&translation_container_, this);
  views::MdTextButton* const translate_toggle =
      translation_view_wrapper_.GetTranslateToggleButtonForTesting();

  bool languages_match = GetParam().target_language_code == kEnglishLanguage;
  bool translate_enabled =
      GetParam().translate_allowed && GetParam().translate_enabled;
  VerifyTranslationItemsVisible(
      &translation_view_wrapper_,
      /*source_language_visible=*/translate_enabled && !languages_match,
      /*target_language_visible=*/translate_enabled);
  EXPECT_EQ(translate_toggle->GetVisible(), GetParam().translate_allowed);
  EXPECT_EQ(translate_toggle->GetText(),
            l10n_util::GetStringUTF16(GetParam().toggle_text_id));
}

TEST_F(TranslationViewWrapperImplTest, ClickToStartTranslation) {
  caption_bubble_settings_.SetTranslateAllowed(true);
  caption_bubble_settings_.SetLiveTranslateEnabled(false);
  caption_bubble_settings_.SetLiveTranslateTargetLanguageCode(kFrenchLanguage);

  translation_view_wrapper_.Init(&translation_container_, this);
  translation_view_wrapper_.SimulateTranslateToggleButtonClickForTesting();
  views::MdTextButton* const translate_toggle =
      translation_view_wrapper_.GetTranslateToggleButtonForTesting();

  VerifyTranslationItemsVisible(&translation_view_wrapper_,
                                /*source_language_visible=*/true,
                                /*target_language_visible=*/true);
  EXPECT_EQ(translate_toggle->GetVisible(), true);
  EXPECT_EQ(translate_toggle->GetText(),
            l10n_util::GetStringUTF16(
                IDS_BOCA_CAPTIONS_STOP_TRANSLATING_BUTTON_TEXT));
}

TEST_F(TranslationViewWrapperImplTest, ClickToStopTranslation) {
  caption_bubble_settings_.SetTranslateAllowed(true);
  caption_bubble_settings_.SetLiveTranslateEnabled(true);
  caption_bubble_settings_.SetLiveTranslateTargetLanguageCode(kFrenchLanguage);

  translation_view_wrapper_.Init(&translation_container_, this);
  translation_view_wrapper_.SimulateTranslateToggleButtonClickForTesting();
  views::MdTextButton* const translate_toggle =
      translation_view_wrapper_.GetTranslateToggleButtonForTesting();

  VerifyTranslationItemsVisible(&translation_view_wrapper_,
                                /*source_language_visible=*/false,
                                /*target_language_visible=*/false);
  EXPECT_EQ(translate_toggle->GetVisible(), true);
  EXPECT_EQ(translate_toggle->GetText(),
            l10n_util::GetStringUTF16(
                IDS_BOCA_CAPTIONS_TRANSLATION_AVAILABLE_BUTTON_TEXT));
}

INSTANTIATE_TEST_SUITE_P(
    TranslationViewWrapperImplTestSuite,
    TranslationViewWrapperImplTest,
    testing::ValuesIn<TranslationViewWrapperImplTestCase>({
        {.test_name = "AllDisabled",
         .translate_allowed = false,
         .translate_enabled = false},
        {.test_name = "TranslateDisabledAndAllowed",
         .translate_allowed = true,
         .translate_enabled = false},
        {.test_name = "TranslateEnabledAndDisallowed",
         .translate_allowed = false,
         .translate_enabled = true},
        {.test_name = "TranslateEnabledAndAllowed",
         .translate_allowed = true,
         .translate_enabled = true,
         .toggle_text_id = IDS_BOCA_CAPTIONS_STOP_TRANSLATING_BUTTON_TEXT},
        {.test_name = "TranslateEnabledAndAllowedSameLanguage",
         .translate_allowed = true,
         .translate_enabled = true,
         .target_language_code = kEnglishLanguage,
         .toggle_text_id = IDS_BOCA_CAPTIONS_STOP_TRANSLATING_BUTTON_TEXT},
    }),
    [](const testing::TestParamInfo<TranslationViewWrapperImplTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace
}  // namespace ash::babelorca
