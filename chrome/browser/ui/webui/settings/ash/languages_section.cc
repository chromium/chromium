// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/languages_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/input_method_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kEditDictionarySubpagePath;
using ::chromeos::settings::mojom::kInputMethodOptionsSubpagePath;
using ::chromeos::settings::mojom::kInputSubpagePath;
using ::chromeos::settings::mojom::kJapaneseManageUserDictionarySubpagePath;
using ::chromeos::settings::mojom::kLanguagesAndInputSectionPath;
using ::chromeos::settings::mojom::kLanguagesSubpagePath;
using ::chromeos::settings::mojom::kSmartInputsSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetLanguagesPageSearchConceptsV2() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_LANGUAGES,
       mojom::kLanguagesSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kLanguages}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_CHANGE_DEVICE_LANGUAGE,
       mojom::kLanguagesSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeDeviceLanguage}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_INPUT_ADD_LANGUAGE,
       mojom::kLanguagesSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddLanguage}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_OFFER_TRANSLATION,
       mojom::kLanguagesSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kOfferTranslation}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetInputPageSearchConceptsV2() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_INPUT,
       mojom::kInputSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kInput}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_INPUT_INPUT_OPTIONS_SHELF,
       mojom::kInputSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowInputOptionsInShelf},
       {IDS_OS_SETTINGS_TAG_LANGUAGES_INPUT_INPUT_OPTIONS_SHELF_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_ADD_INPUT_METHOD,
       mojom::kInputSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddInputMethod}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_SPELL_CHECK,
       mojom::kInputSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSpellCheck}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAutoCorrectionSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_LANGUAGES_AUTO_CORRECTION,
       mojom::kInputMethodOptionsSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowPKAutoCorrection}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetEditDictionarySearchConceptsV2() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_LANGUAGES_EDIT_DICTIONARY,
       mojom::kEditDictionarySubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kEditDictionary}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetSmartInputsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_LANGUAGES_SUGGESTIONS,
       mojom::kSmartInputsSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kSmartInputs}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetEmojiSuggestionSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_LANGUAGES_EMOJI_SUGGESTIONS,
       mojom::kSmartInputsSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowEmojiSuggestions}},
  });
  return *tags;
}

// TODO(crbug/1113611): As Smart Inputs page is renamed to Suggestions.
// All related strings, function names and filenames should be renamed as well.
void AddSmartInputsStrings(content::WebUIDataSource* html_source,
                           bool is_emoji_suggestion_allowed) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"smartInputsTitle", IDS_SETTINGS_SUGGESTIONS_TITLE},
      {"emojiSuggestionTitle", IDS_SETTINGS_SUGGESTIONS_EMOJI_SUGGESTION_TITLE},
      {"emojiSuggestionDescription",
       IDS_SETTINGS_SUGGESTIONS_EMOJI_SUGGESTION_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("allowEmojiSuggestion", is_emoji_suggestion_allowed);
}

void AddInputMethodOptionsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"inputMethodOptionsBasicSectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_BASIC},
      {"inputMethodOptionsAdvancedSectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ADVANCED},
      {"inputMethodOptionsPhysicalKeyboardSectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PHYSICAL_KEYBOARD},
      {"inputMethodOptionsVirtualKeyboardSectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIRTUAL_KEYBOARD},
      {"inputMethodOptionsSuggestionsSectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_SUGGESTIONS},
      {"inputMethodOptionsInputAssistanceSectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_INPUT_ASSISTANCE},
      {"inputMethodOptionsSuggestionsSectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_SUGGESTIONS},
      {"inputMethodOptionsUserDictionariesSectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_USER_DICTIONARIES},
      {"inputMethodOptionsPrivacySectionTitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PRIVACY},
      {"inputMethodOptionsJapaneseAutomaticallySwitchToHalfwidth",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_AUTOMATICALLY_SWITCH_TO_HALFWIDTH},
      {"inputMethodOptionsJapaneseShiftKeyModeStyle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SHIFT_KEY_MODE_STYLE},
      {"inputMethodOptionsJapaneseShiftKeyModeStyleOff",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SHIFT_KEY_MODE_STYLE_OFF},
      {"inputMethodOptionsJapaneseShiftKeyModeStyleAlphanumeric",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SHIFT_KEY_MODE_STYLE_ALPHANUMERIC},
      {"inputMethodOptionsJapaneseShiftKeyModeStyleKatakana",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SHIFT_KEY_MODE_STYLE_KATAKANA},
      {"inputMethodOptionsJapaneseUseInputHistory",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_USE_INPUT_HISTORY},
      {"inputMethodOptionsJapaneseUseSystemDictionary",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_USE_SYSTEM_DICTIONARY},
      {"inputMethodOptionsJapaneseNumberOfSuggestions",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_NUMBER_OF_SUGGESTIONS},
      {"inputMethodOptionsJapaneseDisablePersonalizedSuggestions",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_DISABLE_PERSONALIZED_SUGGESTIONS},
      {"inputMethodOptionsJapaneseAutomaticallySendStatisticsToGoogle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SEND_STATISTICS_TO_GOOGLE},
      {"inputMethodOptionsEnableDoubleSpacePeriod",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ENABLE_DOUBLE_SPACE_PERIOD},
      {"inputMethodOptionsEnableGestureTyping",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ENABLE_GESTURE_TYPING},
      {"inputMethodOptionsEnablePrediction",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ENABLE_PREDICTION},
      {"inputMethodOptionsEnableSoundOnKeypress",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ENABLE_SOUND_ON_KEYPRESS},
      {"inputMethodOptionsEnableCapitalization",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ENABLE_CAPITALIZATION},
      {"inputMethodOptionsAutoCorrection",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_AUTO_CORRECTION},
      {"inputMethodOptionsPredictiveWriting",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PREDICTIVE_WRITING},
      {"inputMethodOptionsDiacriticsOnPhysicalKeyboardLongpress",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_DIACRITICS_ON_PHYSICAL_KEYBOARD_LONGPRESS},
      {"inputMethodOptionsDiacriticsOnPhysicalKeyboardLongpressSubtitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_DIACRITICS_ON_PHYSICAL_KEYBOARD_LONGPRESS_SUBTITLE},
      {"inputMethodOptionsXkbLayout",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_XKB_LAYOUT},
      {"inputMethodOptionsZhuyinKeyboardLayout",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ZHUYIN_KEYBOARD_LAYOUT},
      {"inputMethodOptionsZhuyinSelectKeys",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ZHUYIN_SELECT_KEYS},
      {"inputMethodOptionsZhuyinPageSize",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ZHUYIN_PAGE_SIZE},
      {"inputMethodOptionsEditUserDict",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_EDIT_USER_DICT},
      {"inputMethodOptionsJapaneseInputMode",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_INPUT_MODE},
      {"inputMethodOptionsJapaneseInputModeRomaji",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_INPUT_MODE_ROMAJI},
      {"inputMethodOptionsJapaneseInputModeKana",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_INPUT_MODE_KANA},
      {"inputMethodOptionsJapanesePunctuationStyle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_PUNCTUATION_STYLE},
      {"inputMethodOptionsJapanesePunctuationStyleKutenTouten",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_PUNCTUATION_STYLE_KUTEN_TOUTEN},
      {"inputMethodOptionsJapanesePunctuationStyleCommaPeriod",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_PUNCTUATION_STYLE_COMMA_PERIOD},
      {"inputMethodOptionsJapanesePunctuationStyleKutenPeriod",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_PUNCTUATION_STYLE_KUTEN_PERIOD},
      {"inputMethodOptionsJapanesePunctuationStyleCommaTouten",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_PUNCTUATION_STYLE_COMMA_TOUTEN},
      {"inputMethodOptionsJapaneseSymbolStyle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SYMBOL_STYLE},
      {"inputMethodOptionsJapaneseSymbolStyleCornerBracketMiddleDot",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SYMBOL_STYLE_CORNER_BRACKET_MIDDLE_DOT},
      {"inputMethodOptionsJapaneseSymbolStyleSquareBracketSlash",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SYMBOL_STYLE_SQUARE_BRACKET_SLASH},
      {"inputMethodOptionsJapaneseSymbolStyleCornerBracketSlash",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SYMBOL_STYLE_CORNER_BRACKET_SLASH},
      {"inputMethodOptionsJapaneseSymbolStyleSquareBracketMiddleDot",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SYMBOL_STYLE_SQUARE_BRACKET_MIDDLE_DOT},
      {"inputMethodOptionsJapaneseSpaceInputStyle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SPACE_INPUT_STYLE},
      {"inputMethodOptionsJapaneseSpaceInputStyleInputMode",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SPACE_INPUT_STYLE_INPUT_MODE},
      {"inputMethodOptionsJapaneseSpaceInputStyleFullwidth",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SPACE_INPUT_STYLE_FULLWIDTH},
      {"inputMethodOptionsJapaneseSpaceInputStyleHalfwidth",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SPACE_INPUT_STYLE_HALFWIDTH},
      {"inputMethodOptionsJapaneseSectionShortcut",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SECTION_SHORTCUT},
      {"inputMethodOptionsJapaneseSectionShortcutNoShortcut",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SECTION_SHORTCUT_NO_SHORTCUT},
      {"inputMethodOptionsJapaneseSectionShortcut123456789",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SECTION_SHORTCUT_123456789},
      {"inputMethodOptionsJapaneseSectionShortcutAsdfghjkl",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SECTION_SHORTCUT_ASDFGHJKL},
      {"inputMethodOptionsJapaneseKeymapStyle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE},
      {"inputMethodOptionsJapaneseKeymapStyleCustom",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_CUSTOM},
      {"inputMethodOptionsJapaneseKeymapStyleAtok",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_ATOK},
      {"inputMethodOptionsJapaneseKeymapStyleMsIme",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_MSIME},
      {"inputMethodOptionsJapaneseKeymapStyleKotoeri",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_KOTOERI},
      {"inputMethodOptionsJapaneseKeymapStyleMobile",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_MOBILE},
      {"inputMethodOptionsJapaneseKeymapStyleChromeOs",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_CHROMEOS},
      {"inputMethodOptionsJapaneseManageUserDictionary",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_MANAGE_USER_DICTIONARY},
      {"inputMethodOptionsJapaneseClearPersonalizationData",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_CLEAR_PERSONALIZATION_DATA},
      {"inputMethodOptionsJapaneseManageUserDictionarySubtitle",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_MANAGE_USER_DICTIONARY_SUBTITLE},
      {"inputMethodOptionsPinyinChinesePunctuation",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PINYIN_CHINESE_PUNCTUATION},
      {"inputMethodOptionsPinyinDefaultChinese",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PINYIN_DEFAULT_CHINESE},
      {"inputMethodOptionsPinyinEnableFuzzy",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PINYIN_ENABLE_FUZZY},
      {"inputMethodOptionsPinyinEnableLowerPaging",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PINYIN_ENABLE_LOWER_PAGING},
      {"inputMethodOptionsPinyinEnableUpperPaging",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PINYIN_ENABLE_UPPER_PAGING},
      {"inputMethodOptionsPinyinFullWidthCharacter",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_PINYIN_FULL_WIDTH_CHARACTER},
      {"inputMethodOptionsAutoCorrectionOff",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_AUTO_CORRECTION_OFF},
      {"inputMethodOptionsAutoCorrectionModest",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_AUTO_CORRECTION_MODEST},
      {"inputMethodOptionsAutoCorrectionAggressive",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_AUTO_CORRECTION_AGGRESSIVE},
      {"inputMethodOptionsUsKeyboard",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_KEYBOARD_US},
      {"inputMethodOptionsZhuyinLayoutDefault",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ZHUYIN_LAYOUT_DEFAULT},
      {"inputMethodOptionsZhuyinLayoutIBM",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ZHUYIN_LAYOUT_IBM},
      {"inputMethodOptionsZhuyinLayoutEten",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_ZHUYIN_LAYOUT_ETEN},
      {"inputMethodOptionsKoreanLayout",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_KOREAN_LAYOUT},
      {"inputMethodOptionsKoreanSyllableInput",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_KOREAN_SYLLABLE_INPUT},
      {"inputMethodOptionsDvorakKeyboard",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_KEYBOARD_DVORAK},
      {"inputMethodOptionsColemakKeyboard",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_KEYBOARD_COLEMAK},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean(
      "allowPredictiveWriting",
      base::FeatureList::IsEnabled(features::kAssistMultiWord));
  html_source->AddBoolean(
      "allowDiacriticsOnPhysicalKeyboardLongpress",
      base::FeatureList::IsEnabled(
          features::kDiacriticsOnPhysicalKeyboardLongpress));
  html_source->AddBoolean(
      "allowAutocorrectToggle",
      base::FeatureList::IsEnabled(features::kAutocorrectToggle));
  html_source->AddBoolean(
      "autocorrectEnableByDefault",
      base::FeatureList::IsEnabled(features::kAutocorrectByDefault));
}

void AddLanguagesPageStringsV2(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"deviceLanguageTitle", IDS_OS_SETTINGS_LANGUAGES_DEVICE_LANGUAGE_TITLE},
      {"changeDeviceLanguageLabel",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_BUTTON_LABEL},
      {"changeDeviceLanguageButtonDescription",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_BUTTON_DESCRIPTION},
      {"languagesPreferenceTitle",
       IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PREFERENCE_TITLE},
      {"websiteLanguagesTitle",
       IDS_OS_SETTINGS_LANGUAGES_WEBSITE_LANGUAGES_TITLE},
      {"offerToTranslateThisLanguage",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TO_TRANSLATE_THIS_LANGUAGE},
      {"offerTranslationLabel",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TRANSLATION_LABEL},
      {"offerTranslationSublabel",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TRANSLATION_SUBLABEL},
      {"offerGoogleTranslateLabel",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_GOOGLE_TRANSLATE_LABEL},
      {"changeDeviceLanguageDialogTitle",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_DIALOG_TITLE},
      {"selectedDeviceLanguageInstruction",
       IDS_OS_SETTINGS_LANGUAGES_SELECTED_DEVICE_LANGUAGE_INSTRUCTION},
      {"notSelectedDeviceLanguageInstruction",
       IDS_OS_SETTINGS_LANGUAGES_NOT_SELECTED_DEVICE_LANGUAGE_INSTRUCTION},
      {"changeDeviceLanguageConfirmButtonLabel",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_CONFIRM_BUTTON_LABEL},
      {"googleAccountLanguageTitle",
       IDS_OS_SETTINGS_LANGUAGES_GOOGLE_ACCOUNT_LANGUAGE_TITLE},
      {"googleAccountLanguageDescription",
       IDS_OS_SETTINGS_LANGUAGES_GOOGLE_ACCOUNT_LANGUAGE_DESCRIPTION},
      {"manageGoogleAccountLanguageLabel",
       IDS_OS_SETTINGS_LANGUAGES_MANAGE_GOOGLE_ACCOUNT_LANGUAGE_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "languagesPreferenceDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PREFERENCE_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl)));
  html_source->AddString(
      "websiteLanguagesDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_LANGUAGES_WEBSITE_LANGUAGES_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl)));
  html_source->AddString(
      "translateTargetLabel",
      l10n_util::GetStringUTF16(
          QuickAnswersState::Get() && QuickAnswersState::Get()->is_eligible()
              ? IDS_OS_SETTINGS_LANGUAGES_TRANSLATE_TARGET_LABEL_WITH_QUICK_ANSWERS
              : IDS_OS_SETTINGS_LANGUAGES_TRANSLATE_TARGET_LABEL));
  html_source->AddString(
      "changeDeviceLanguageDialogDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_DIALOG_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl)));

  html_source->AddString(
      "googleAccountLanguagesURL",
      net::AppendQueryParameter(GURL(chrome::kGoogleAccountLanguagesURL),
                                "utm_source", "chrome-settings")
          .spec());
}

void AddInputPageStringsV2(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"inputMethodListTitle",
       IDS_OS_SETTINGS_LANGUAGES_INPUT_METHOD_LIST_TITLE},
      {"openOptionsPage", IDS_OS_SETTINGS_LANGUAGES_OPEN_OPTIONS_PAGE_LABEL},
      {"addInputMethodLabel", IDS_OS_SETTINGS_LANGUAGES_ADD_INPUT_METHOD_LABEL},
      {"removeInputMethodTooltip",
       IDS_OS_SETTINGS_LANGUAGES_REMOVE_INPUT_METHOD_TOOLTIP},
      {"suggestedInputMethodsLabel",
       IDS_OS_SETTINGS_LANGUAGES_SUGGESTED_INPUT_METHODS_LABEL},
      {"allInputMethodsLabel",
       IDS_OS_SETTINGS_LANGUAGES_ALL_INPUT_METHODS_LABEL},
      {"searchInputMethodsLabel",
       IDS_OS_SETTINGS_LANGUAGES_SEARCH_INPUT_METHODS_LABEL},
      {"inputMethodNotAllowed",
       IDS_OS_SETTINGS_LANGUAGES_INPUT_METHOD_NOT_ALLOWED},
      {"spellCheckTitle", IDS_OS_SETTINGS_LANGUAGES_SPELL_CHECK_TITLE},
      {"spellAndGrammarCheckTitle",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_AND_GRAMMAR_CHECK_TITLE},
      {"spellAndGrammarCheckDescription",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_AND_GRAMMAR_CHECK_DESCRIPTION},
      {"spellCheckEnhancedLabel",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_CHECK_ENHANCED_LABEL},
      {"spellCheckLanguagesListTitle",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_CHECK_LANGUAGES_LIST_TITLE},
      {"spellCheckLanguagesListDescription",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_CHECK_LANGUAGES_LIST_DESCRIPTION},
      {"addSpellCheckLanguagesTitle",
       IDS_OS_SETTINGS_LANGUAGES_ADD_SPELL_CHECK_LANGUAGES_TITLE},
      {"searchSpellCheckLanguagesLabel",
       IDS_OS_SETTINGS_LANGUAGES_SEARCH_SPELL_CHECK_LANGUAGES_LABEL},
      {"suggestedSpellcheckLanguages",
       IDS_OS_SETTINGS_LANGUAGES_SUGGESTED_SPELL_CHECK_LANGUAGES_LABEL},
      {"allSpellcheckLanguages",
       IDS_OS_SETTINGS_LANGUAGES_ALL_SPELL_CHECK_LANGUAGES_LABEL},
      {"spellCheckLanguageNotAllowed",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_CHECK_LANGUAGE_NOT_ALLOWED},
      {"removeSpellCheckLanguageTooltip",
       IDS_OS_SETTINGS_LANGUAGES_REMOVE_SPELL_CHECK_LANGUAGE_TOOLTIP},
      {"languagesDictionaryDownloadError",
       IDS_OS_SETTINGS_LANGUAGES_DICTIONARY_DOWNLOAD_FAILED},
      {"languagesDictionaryDownloadRetryLabel",
       IDS_OS_SETTINGS_LANGUAGES_DICTIONARY_DOWNLOAD_RETRY_LABEL},
      {"languagesDictionaryDownloadRetryDescription",
       IDS_OS_SETTINGS_LANGUAGES_DICTIONARY_DOWNLOAD_RETRY_DESCRIPTION},
      {"editDictionaryLabel", IDS_OS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_LABEL},
      {"japaneseManageUserDictionaryLabel",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY_LABEL},
      {"editDictionaryDescription",
       IDS_OS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_DESCRIPTION},
      {"addDictionaryWordButtonLabel",
       IDS_OS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_BUTTON_LABEL},
      {"addDictionaryWordDuplicateError",
       IDS_OS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_DUPLICATE_ERROR},
      {"addDictionaryWordLengthError",
       IDS_OS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_LENGTH_ERROR},
      {"deleteDictionaryWordTooltip",
       IDS_OS_SETTINGS_LANGUAGES_DELETE_DICTIONARY_WORD_TOOLTIP},
      {"noDictionaryWordsLabel",
       IDS_OS_SETTINGS_LANGUAGES_NO_DICTIONARY_WORDS_LABEL},
      {"imeShortcutReminderTitle",
       IDS_OS_SETTINGS_LANGUAGES_SHORTCUT_REMINDER_TITLE},
      {"imeShortcutReminderLastUsed",
       IDS_OS_SETTINGS_LANGUAGES_SHORTCUT_REMINDER_LAST_USED_IME_DESCRIPTION},
      {"imeShortcutReminderNext",
       IDS_OS_SETTINGS_LANGUAGES_SHORTCUT_REMINDER_NEXT_IME_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace

LanguagesSection::LanguagesSection(Profile* profile,
                                   SearchTagRegistry* search_tag_registry,
                                   PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckEnable,
      base::BindRepeating(&LanguagesSection::UpdateSpellCheckSearchTags,
                          base::Unretained(this)));
  observation_.Observe(input_method::InputMethodManager::Get());

  updater.AddSearchTags(GetLanguagesPageSearchConceptsV2());
  updater.AddSearchTags(GetInputPageSearchConceptsV2());
  UpdateSpellCheckSearchTags();

  if (IsEmojiSuggestionAllowed()) {
    updater.AddSearchTags(GetSmartInputsSearchConcepts());
    if (IsEmojiSuggestionAllowed())
      updater.AddSearchTags(GetEmojiSuggestionSearchConcepts());
  }
  updater.AddSearchTags(GetAutoCorrectionSearchConcepts());
}

LanguagesSection::~LanguagesSection() = default;

void LanguagesSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"osLanguagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE},
      {"languagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PAGE_TITLE},
      {"inputPageTitle", IDS_OS_SETTINGS_LANGUAGES_INPUT_PAGE_TITLE},
      {"inputPageTitleV2", IDS_OS_SETTINGS_LANGUAGES_INPUT_PAGE_TITLE_V2},
      {"inputMethodEnabled", IDS_SETTINGS_LANGUAGES_INPUT_METHOD_ENABLED},
      {"inputMethodsManagedbyPolicy",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGED_BY_POLICY},
      {"showImeMenu", IDS_SETTINGS_LANGUAGES_SHOW_IME_MENU},
      {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
      {"searchLanguages", IDS_SETTINGS_LANGUAGE_SEARCH},
      {"addLanguagesDialogTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
      {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
      {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
      {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
      {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddString(
      "languagePacksNotice",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_LANGUAGES_LANGUAGE_PACKS_NOTICE,
          base::ASCIIToUTF16(chrome::kLanguagePacksLearnMoreURL)));
  AddSmartInputsStrings(html_source, IsEmojiSuggestionAllowed());
  AddInputMethodOptionsStrings(html_source);
  AddLanguagesPageStringsV2(html_source);
  AddInputPageStringsV2(html_source);

  html_source->AddBoolean("enableLanguageSettingsV2Update2", true);
  html_source->AddBoolean(
      "onDeviceGrammarCheckEnabled",
      base::FeatureList::IsEnabled(features::kOnDeviceGrammarCheck));
  html_source->AddBoolean("languagePacksHandwritingEnabled",
                          features::IsLanguagePacksEnabled());
  html_source->AddBoolean(
      "systemJapanesePhysicalTyping",
      base::FeatureList::IsEnabled(features::kSystemJapanesePhysicalTyping));
}

void LanguagesSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::LanguagesHandler>(profile()));
}

int LanguagesSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE;
}

mojom::Section LanguagesSection::GetSection() const {
  return mojom::Section::kLanguagesAndInput;
}

mojom::SearchResultIcon LanguagesSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kGlobe;
}

std::string LanguagesSection::GetSectionPath() const {
  return mojom::kLanguagesAndInputSectionPath;
}

bool LanguagesSection::LogMetric(mojom::Setting setting,
                                 base::Value& value) const {
  // Unimplemented.
  return false;
}

void LanguagesSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // Languages.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PAGE_TITLE,
      mojom::Subpage::kLanguages, mojom::SearchResultIcon::kGlobe,
      mojom::SearchResultDefaultRank::kMedium, mojom::kLanguagesSubpagePath);
  static constexpr mojom::Setting kLanguagesPageSettings[] = {
      mojom::Setting::kChangeDeviceLanguage,
      mojom::Setting::kOfferTranslation,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kLanguages, kLanguagesPageSettings,
                            generator);

  // Input.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_LANGUAGES_INPUT_PAGE_TITLE, mojom::Subpage::kInput,
      mojom::SearchResultIcon::kGlobe, mojom::SearchResultDefaultRank::kMedium,
      mojom::kInputSubpagePath);
  static constexpr mojom::Setting kInputPageSettings[] = {
      mojom::Setting::kAddInputMethod,
      mojom::Setting::kSpellCheck,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kInput, kInputPageSettings,
                            generator);

  // Edit dictionary.
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_LABEL,
      mojom::Subpage::kEditDictionary, mojom::Subpage::kInput,
      mojom::SearchResultIcon::kGlobe, mojom::SearchResultDefaultRank::kMedium,
      mojom::kEditDictionarySubpagePath);

  // Japanese Manage User Dictionary
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY_LABEL,
      mojom::Subpage::kJapaneseManageUserDictionary, mojom::Subpage::kInput,
      mojom::SearchResultIcon::kGlobe, mojom::SearchResultDefaultRank::kMedium,
      mojom::kJapaneseManageUserDictionarySubpagePath);

  generator->RegisterNestedSetting(mojom::Setting::kAddLanguage,
                                   mojom::Subpage::kLanguages);
  generator->RegisterNestedSetting(mojom::Setting::kShowInputOptionsInShelf,
                                   mojom::Subpage::kInput);

  // Input method options.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_LANGUAGES_INPUT_METHOD_OPTIONS_TITLE,
      mojom::Subpage::kInputMethodOptions, mojom::Subpage::kInput,
      mojom::SearchResultIcon::kGlobe, mojom::SearchResultDefaultRank::kMedium,
      mojom::kInputMethodOptionsSubpagePath);

  generator->RegisterNestedSetting(mojom::Setting::kShowPKAutoCorrection,
                                   mojom::Subpage::kInputMethodOptions);
  generator->RegisterNestedSetting(mojom::Setting::kShowVKAutoCorrection,
                                   mojom::Subpage::kInputMethodOptions);

  // Smart inputs.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_SUGGESTIONS_TITLE, mojom::Subpage::kSmartInputs,
      mojom::SearchResultIcon::kGlobe, mojom::SearchResultDefaultRank::kMedium,
      mojom::kSmartInputsSubpagePath);
  static constexpr mojom::Setting kSmartInputsFeaturesSettings[] = {
      mojom::Setting::kShowEmojiSuggestions,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSmartInputs,
                            kSmartInputsFeaturesSettings, generator);
}

bool LanguagesSection::IsEmojiSuggestionAllowed() const {
  return pref_service_->GetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed);
}

bool LanguagesSection::IsSpellCheckEnabled() const {
  return pref_service_->GetBoolean(spellcheck::prefs::kSpellCheckEnable);
}

void LanguagesSection::UpdateSpellCheckSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetEditDictionarySearchConceptsV2());
  if (IsSpellCheckEnabled()) {
    updater.AddSearchTags(GetEditDictionarySearchConceptsV2());
  }
}

void LanguagesSection::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  DCHECK(manager);
  const std::string engine_id =
      extension_ime_util::GetComponentIDByInputMethodID(
          manager->GetActiveIMEState()->GetCurrentInputMethod().id());
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetAutoCorrectionSearchConcepts());
  if (input_method::IsAutocorrectSupported(engine_id)) {
    updater.AddSearchTags(GetAutoCorrectionSearchConcepts());
  }
}

}  // namespace ash::settings
