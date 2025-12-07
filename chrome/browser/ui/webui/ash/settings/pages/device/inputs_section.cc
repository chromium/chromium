// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/inputs_section.h"

#include <array>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/ash/input_method/input_method_settings.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kDeviceSectionPath;
using ::chromeos::settings::mojom::kEditDictionarySubpagePath;
using ::chromeos::settings::mojom::kInputMethodOptionsSubpagePath;
using ::chromeos::settings::mojom::kInputSubpagePath;
using ::chromeos::settings::mojom::kJapaneseManageUserDictionarySubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

base::span<const SearchConcept> GetDefaultSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_INPUT,
       mojom::kInputSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kInput}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_INPUT_INPUT_OPTIONS_SHELF,
       mojom::kInputSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowInputOptionsInShelf},
       {IDS_OS_SETTINGS_TAG_LANGUAGES_INPUT_INPUT_OPTIONS_SHELF_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_ADD_INPUT_METHOD,
       mojom::kInputSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddInputMethod}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_SPELL_CHECK,
       mojom::kInputSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSpellCheckOnOff}},
  });
  return tags;
}

base::span<const SearchConcept> GetSpellCheckSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_LANGUAGES_EDIT_DICTIONARY,
       mojom::kEditDictionarySubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kEditDictionary}},
  });
  return tags;
}

base::span<const SearchConcept> GetAutoCorrectionSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_LANGUAGES_AUTO_CORRECTION,
       mojom::kInputMethodOptionsSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowPKAutoCorrection}},
  });
  return tags;
}

void AddInputMethodOptionsLoadTimeData(
    content::WebUIDataSource* html_source,
    bool is_physical_keyboard_autocorrect_allowed,
    bool is_physical_keyboard_predictive_writing_allowed) {
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
      {"inputMethodOptionsVietnameseShorthandTypingTitle",
       IDS_SETTINGS_INPUT_METHOD_HEADING_SHORTHAND_TYPING},
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
      {"inputMethodOptionsJapaneseKeymapStyleAtok",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_ATOK},
      {"inputMethodOptionsJapaneseKeymapStyleMsIme",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_MSIME},
      {"inputMethodOptionsJapaneseKeymapStyleKotoeri",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_KOTOERI},
      {"inputMethodOptionsJapaneseKeymapStyleChromeOs",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_KEYMAP_STYLE_CHROMEOS},
      {"inputMethodOptionsJapaneseManageUserDictionary",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_MANAGE_USER_DICTIONARY},
      {"inputMethodOptionsJapaneseDeletePersonalizationData",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_DELETE_PERSONALIZATION_DATA},
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
      {"inputMethodOptionsVietnameseModernToneMarkPlacement",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIETNAMESE_MODERN_TONE_MARK_PLACEMENT},
      {"inputMethodOptionsVietnameseModernToneMarkPlacementDescription",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_DESCRIPTION_VIETNAMESE_MODERN_TONE_MARK_PLACEMENT},
      {"inputMethodOptionsVietnameseFlexibleTyping",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIETNAMESE_FLEXIBLE_TYPING},
      {"inputMethodOptionsVietnameseTelexFlexibleTypingDescription",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIETNAMESE_TELEX_FLEXIBLE_TYPING_DESCRIPTION},
      {"inputMethodOptionsVietnameseVniFlexibleTypingDescription",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIETNAMESE_VNI_FLEXIBLE_TYPING_DESCRIPTION},
      {"inputMethodOptionsVietnameseVniUoHookShortcut",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIETNAMESE_VNI_UO_HOOK_SHORTCUT},
      {"inputMethodOptionsVietnameseTelexUoHookShortcut",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIETNAMESE_TELEX_UO_HOOK_SHORTCUT},
      {"inputMethodOptionsVietnameseTelexWShortcut",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIETNAMESE_TELEX_W_SHORTCUT},
      {"inputMethodOptionsVietnameseShowUnderline",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_VIETNAMESE_SHOW_UNDERLINE},
      {"inputMethodOptionsVietnameseShowUnderlineDescription",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_DESCRIPTION_VIETNAMESE_SHOW_UNDERLINE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean("isPhysicalKeyboardAutocorrectAllowed",
                          is_physical_keyboard_autocorrect_allowed);
  html_source->AddBoolean(
      "isPhysicalKeyboardPredictiveWritingAllowed",
      base::FeatureList::IsEnabled(features::kAssistMultiWord) &&
          is_physical_keyboard_predictive_writing_allowed);
  html_source->AddBoolean(
      "autocorrectEnableByDefault",
      base::FeatureList::IsEnabled(features::kAutocorrectByDefault));
  html_source->AddBoolean("allowFirstPartyVietnameseInput", true);
}

}  // namespace

InputsSection::InputsSection(Profile* profile,
                             SearchTagRegistry* search_tag_registry,
                             PrefService* pref_service,
                             input_method::EditorMediator* editor_mediator)
    : OsSettingsSection(profile, search_tag_registry),
      profile_(profile),
      pref_service_(pref_service),
      editor_mediator_(editor_mediator) {
  CHECK(profile);
  CHECK(search_tag_registry);
  CHECK(pref_service);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      spellcheck::prefs::kSpellCheckEnable,
      base::BindRepeating(&InputsSection::UpdateSpellCheckSearchTags,
                          base::Unretained(this)));

  auto* input_method_manager = input_method::InputMethodManager::Get();
  observation_.Observe(input_method_manager);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetDefaultSearchConcepts());

  UpdateSpellCheckSearchTags();

  UpdateAutocorrectTags(input_method_manager);
}

InputsSection::~InputsSection() = default;

void InputsSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  webui::LocalizedString kLocalizedStrings[] = {
      {"inputPageTitle", IDS_OS_SETTINGS_LANGUAGES_INPUT_PAGE_TITLE},
      {"inputMethodEnabled", IDS_SETTINGS_LANGUAGES_INPUT_METHOD_ENABLED},
      {"inputMethodsManagedbyPolicy",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGED_BY_POLICY},
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
      {"inputMethodLanguagePacksGeneralError",
       IDS_OS_SETTINGS_INPUT_METHOD_LANGUAGE_PACKS_GENERAL_ERROR},
      {"inputMethodLanguagePacksNeedsRebootError",
       IDS_OS_SETTINGS_INPUT_METHOD_LANGUAGE_PACKS_NEEDS_REBOOT_ERROR},
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
      {"japaneseClearPersonalizationData",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_CLEAR_PERSONALIZATION_DATA},
      {"japaneseDeleteDictionaryLabel",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_DELETE_DICTIONARY_BUTTON_LABEL},
      {"japaneseDeleteDictionary",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_DELETE_DICTIONARY},
      {"japaneseDeleteDictionaryDetail",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_DELETE_DICTIONARY_DETAIL},
      {"japaneseDeleteDictionaryButton",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_DELETE_DICTIONARY_BUTTON},
      {"japaneseDeleteItems",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_DELETE_ITEMS},
      {"japaneseConversationHistory",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_CONVERSATION_HISTORY},
      {"japaneseSuggestionHistory",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_SUGGESTION_HISTORY},
      {"japaneseManageUserDictionaryLabel",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY_LABEL},
      {"japaneseDictionary",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_JAPANESE_DICTIONARY},
      {"japaneseDictionaryAddDictionary",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_ADD_DICTIONARY},
      {"japaneseDictionaryCategory",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_CATEGORY},
      {"japaneseDictionaryComment",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_COMMENT},
      {"japaneseDictionaryExport",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_EXPORT},
      {"japaneseDictionaryImport",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_IMPORT},
      {"japaneseDictionaryDefaultName",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_DEFAULT_NAME},
      {"japaneseDictionaryName",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_NAME},
      {"japaneseDictionaryEntryPosition",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_ENTRY_POSITION},
      {"japaneseDictionaryEntryDeleted",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_ENTRY_DELETED},
      {"japaneseDictionaryDeleteEntry",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_DELETE_ENTRY},
      {"japaneseDictionaryNewEntry",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_NEW_ENTRY},
      {"japaneseDictionaryReading",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_READING},
      {"japaneseDictionaryWord",
       IDS_OS_SETTINGS_LANGUAGES_JAPANESE_DICTIONARY_WORD},
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
      {"imeCustomizedShortcutReminderLastUsed",
       IDS_OS_SETTINGS_LANGUAGES_CUSTOMIZED_SHORTCUT_REMINDER_LAST_USED_IME_DESCRIPTION},
      {"imeCustomizedShortcutReminderNext",
       IDS_OS_SETTINGS_LANGUAGES_CUSTOMIZED_SHORTCUT_REMINDER_NEXT_IME_DESCRIPTION},
      {"imeShortcutReminderLastUsed",
       IDS_OS_SETTINGS_LANGUAGES_SHORTCUT_REMINDER_LAST_USED_IME_DESCRIPTION},
      {"imeShortcutReminderNext",
       IDS_OS_SETTINGS_LANGUAGES_SHORTCUT_REMINDER_NEXT_IME_DESCRIPTION},
      {"showImeMenu", IDS_SETTINGS_LANGUAGES_SHOW_IME_MENU},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "languagePacksNotice",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_LANGUAGES_LANGUAGE_PACKS_NOTICE,
                                 chrome::kLanguagePacksLearnMoreURL));
  html_source->AddBoolean(
      "onDeviceGrammarCheckEnabled",
      base::FeatureList::IsEnabled(features::kOnDeviceGrammarCheck));

  html_source->AddBoolean(
      "languagePacksInSettingsEnabled",
      base::FeatureList::IsEnabled(features::kLanguagePacksInSettings));
  // TODO(b/290861003): Update the settings code and remove this.
  html_source->AddBoolean("isShortcutCustomizationEnabled", true);

  AddInputMethodOptionsLoadTimeData(
      html_source,
      input_method::IsPhysicalKeyboardAutocorrectAllowed(*pref_service_),
      input_method::IsPhysicalKeyboardPredictiveWritingAllowed(*pref_service_));
}

void InputsSection::AddHandlers(content::WebUI* web_ui) {
  // No handlers registered.
}

int InputsSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_LANGUAGES_INPUT_PAGE_TITLE;
}

mojom::Section InputsSection::GetSection() const {
  // Note: This is a subsection that exists under the Device section. This is
  // not a top-level section and does not have a respective declaration in
  // chromeos::settings::mojom::Section.
  return mojom::Section::kDevice;
}

mojom::SearchResultIcon InputsSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kLanguage;
}

const char* InputsSection::GetSectionPath() const {
  return mojom::kDeviceSectionPath;
}

bool InputsSection::LogMetric(mojom::Setting setting,
                              base::Value& value) const {
  // No metrics are logged.
  return false;
}

void InputsSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_LANGUAGES_INPUT_PAGE_TITLE, mojom::Subpage::kInput,
      mojom::SearchResultIcon::kLanguage,
      mojom::SearchResultDefaultRank::kMedium, mojom::kInputSubpagePath);
  static constexpr mojom::Setting kInputSubpageSettings[] = {
      mojom::Setting::kAddInputMethod,
      mojom::Setting::kRemoveInputMethod,
      mojom::Setting::kSetCurrentInputMethod,
      mojom::Setting::kShowInputOptionsInShelf,
      mojom::Setting::kSpellCheckOnOff,
      mojom::Setting::kAddSpellCheckLanguage,
      mojom::Setting::kRemoveSpellCheckLanguage,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kInput, kInputSubpageSettings,
                            generator);

  // Edit dictionary subpage
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_LABEL,
      mojom::Subpage::kEditDictionary, mojom::Subpage::kInput,
      mojom::SearchResultIcon::kLanguage,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kEditDictionarySubpagePath);
  static constexpr mojom::Setting kEditDictionarySubpageSettings[] = {
      mojom::Setting::kAddSpellCheckWord,
      mojom::Setting::kRemoveSpellCheckWord,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kEditDictionary,
                            kEditDictionarySubpageSettings, generator);

  // Japanese Manage User Dictionary subpage
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY_LABEL,
      mojom::Subpage::kJapaneseManageUserDictionary, mojom::Subpage::kInput,
      mojom::SearchResultIcon::kLanguage,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kJapaneseManageUserDictionarySubpagePath);

  // Input method options subpage
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_LANGUAGES_INPUT_METHOD_OPTIONS_TITLE,
      mojom::Subpage::kInputMethodOptions, mojom::Subpage::kInput,
      mojom::SearchResultIcon::kLanguage,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kInputMethodOptionsSubpagePath);
  static constexpr mojom::Setting kInputMethodOptionsSubpageSettings[] = {
      mojom::Setting::kShowPKAutoCorrection,
      mojom::Setting::kShowVKAutoCorrection,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kInputMethodOptions,
                            kInputMethodOptionsSubpageSettings, generator);
}

void InputsSection::UpdateSpellCheckSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetSpellCheckSearchConcepts());
  if (IsSpellCheckEnabled()) {
    updater.AddSearchTags(GetSpellCheckSearchConcepts());
  }
}

void InputsSection::UpdateAutocorrectTags(
    input_method::InputMethodManager* manager) {
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

void InputsSection::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  UpdateAutocorrectTags(manager);
}

bool InputsSection::IsSpellCheckEnabled() const {
  return pref_service_->GetBoolean(spellcheck::prefs::kSpellCheckEnable);
}

}  // namespace ash::settings
