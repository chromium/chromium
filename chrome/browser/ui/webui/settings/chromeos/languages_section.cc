// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/languages_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
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

bool IsLanguageSettingsV2Update2Enabled() {
  return base::FeatureList::IsEnabled(
      ::chromeos::features::kLanguageSettingsUpdate2);
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

const std::vector<SearchConcept>& GetAssistivePersonalInfoSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_LANGUAGES_PERSONAL_INFORMATION_SUGGESTIONS,
       mojom::kSmartInputsSubpagePath,
       mojom::SearchResultIcon::kGlobe,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowPersonalInformationSuggestions}},
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

bool IsAssistivePersonalInfoAllowed() {
  return !features::IsGuestModeActive() &&
         base::FeatureList::IsEnabled(
             ::chromeos::features::kAssistPersonalInfo);
}

// TODO(crbug/1113611): As Smart Inputs page is renamed to Suggestions.
// All related strings, function names and filenames should be renamed as well.
void AddSmartInputsStrings(content::WebUIDataSource* html_source,
                           bool is_emoji_suggestion_allowed) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"smartInputsTitle", IDS_SETTINGS_SUGGESTIONS_TITLE},
      {"personalInfoSuggestionTitle",
       IDS_SETTINGS_SUGGESTIONS_PERSONAL_INFO_TITLE},
      {"personalInfoSuggestionHelpTooltip",
       IDS_SETTINGS_SUGGESTIONS_PERSONAL_INFO_HELP_TOOLTIP},
      {"personalInfoSuggestionDescription",
       IDS_SETTINGS_SUGGESTIONS_PERSONAL_INFO_DESCRIPTION},
      {"managePersonalInfo", IDS_SETTINGS_SUGGESTIONS_MANAGE_PERSONAL_INFO},
      {"emojiSuggestionTitle", IDS_SETTINGS_SUGGESTIONS_EMOJI_SUGGESTION_TITLE},
      {"emojiSuggestionDescription",
       IDS_SETTINGS_SUGGESTIONS_EMOJI_SUGGESTION_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("allowAssistivePersonalInfo",
                          IsAssistivePersonalInfoAllowed());
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
      {"inputMethodOptionsXkbLayout",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_XKB_LAYOUT},
      {"inputMethodOptionsEditUserDict",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_EDIT_USER_DICT},
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
      {"inputMethodOptionsDvorakKeyboard",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_KEYBOARD_DVORAK},
      {"inputMethodOptionsColemakKeyboard",
       IDS_SETTINGS_INPUT_METHOD_OPTIONS_KEYBOARD_COLEMAK},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
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
      {"languagesPreferenceDescription",
       IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PREFERENCE_DESCRIPTION},
      {"translateTargetLabel",
       IDS_OS_SETTINGS_LANGUAGES_TRANSLATE_TARGET_LABEL},
      {"offerToTranslateThisLanguage",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TO_TRANSLATE_THIS_LANGUAGE},
      {"offerTranslationLabel",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TRANSLATION_LABEL},
      {"offerTranslationSublabel",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TRANSLATION_SUBLABEL},
      {"changeDeviceLanguageDialogTitle",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_DIALOG_TITLE},
      {"selectedDeviceLanguageInstruction",
       IDS_OS_SETTINGS_LANGUAGES_SELECTED_DEVICE_LANGUAGE_INSTRUCTION},
      {"notSelectedDeviceLanguageInstruction",
       IDS_OS_SETTINGS_LANGUAGES_NOT_SELECTED_DEVICE_LANGUAGE_INSTRUCTION},
      {"changeDeviceLanguageConfirmButtonLabel",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_CONFIRM_BUTTON_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "languagesPreferenceDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PREFERENCE_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl)));
  html_source->AddString(
      "changeDeviceLanguageDialogDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_DIALOG_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl)));
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
      {"spellCheckEnhancedLabel",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_CHECK_ENHANCED_LABEL},
      {"spellCheckLanguagesListTitle",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_CHECK_LANGUAGES_LIST_TITLE},
      {"spellCheckLanguagesListDescription",
       IDS_OS_SETTINGS_LANGUAGES_SPELL_CHECK_LANGUAGES_LIST_DESCRIPTION},
      {"addSpellCheckLanguagesTitle",
       IDS_OS_SETTINGS_LANGUAGES_ADD_SPELL_CHECK_LANGUAGES_TITLE},
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

  updater.AddSearchTags(GetLanguagesPageSearchConceptsV2());
  updater.AddSearchTags(GetInputPageSearchConceptsV2());
  UpdateSpellCheckSearchTags();

  if (IsAssistivePersonalInfoAllowed() || IsEmojiSuggestionAllowed()) {
    updater.AddSearchTags(GetSmartInputsSearchConcepts());
    if (IsAssistivePersonalInfoAllowed())
      updater.AddSearchTags(GetAssistivePersonalInfoSearchConcepts());
    if (IsEmojiSuggestionAllowed())
      updater.AddSearchTags(GetEmojiSuggestionSearchConcepts());
  }
}

LanguagesSection::~LanguagesSection() = default;

void LanguagesSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"orderLanguagesInstructions",
       IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_ORDERING_INSTRUCTIONS},
      {"osLanguagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE},
      {"languagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PAGE_TITLE},
      {"inputPageTitle", IDS_OS_SETTINGS_LANGUAGES_INPUT_PAGE_TITLE},
      {"osLanguagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE},
      {"osLanguagesListTitle", IDS_OS_SETTINGS_LANGUAGES_LIST_TITLE},
      {"inputMethodsListTitle",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_LIST_TITLE},
      {"inputMethodEnabled", IDS_SETTINGS_LANGUAGES_INPUT_METHOD_ENABLED},
      {"inputMethodsExpandA11yLabel",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_EXPAND_ACCESSIBILITY_LABEL},
      {"inputMethodsManagedbyPolicy",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGED_BY_POLICY},
      {"manageInputMethods", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGE},
      {"manageInputMethodsPageTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_INPUT_METHODS_TITLE},
      {"showImeMenu", IDS_SETTINGS_LANGUAGES_SHOW_IME_MENU},
      {"displayLanguageRestart",
       IDS_SETTINGS_LANGUAGES_RESTART_TO_DISPLAY_LANGUAGE},
      {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
      {"displayInThisLanguage",
       IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
      {"searchLanguages", IDS_SETTINGS_LANGUAGE_SEARCH},
      {"addLanguagesDialogTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
      {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
      {"isDisplayedInThisLanguage",
       IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
      {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
      {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
      {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  AddSmartInputsStrings(html_source, IsEmojiSuggestionAllowed());
  AddInputMethodOptionsStrings(html_source);
  AddLanguagesPageStringsV2(html_source);
  AddInputPageStringsV2(html_source);

  html_source->AddString(
      "languagesLearnMoreURL",
      base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl));
  html_source->AddBoolean("imeOptionsInSettings",
                          base::FeatureList::IsEnabled(
                              ::chromeos::features::kImeOptionsInSettings));
  // TODO(crbug.com/1097328): Delete this.
  html_source->AddBoolean("enableLanguageSettingsV2", true);
  html_source->AddBoolean("enableLanguageSettingsV2Update2",
                          IsLanguageSettingsV2Update2Enabled());
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

  // Languages and input details.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE,
      mojom::Subpage::kLanguagesAndInputDetails,
      mojom::SearchResultIcon::kGlobe, mojom::SearchResultDefaultRank::kMedium,
      mojom::kLanguagesAndInputDetailsSubpagePath);

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

  // Manage input methods.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_LANGUAGES_MANAGE_INPUT_METHODS_TITLE,
      mojom::Subpage::kManageInputMethods,
      mojom::Subpage::kLanguagesAndInputDetails,
      mojom::SearchResultIcon::kGlobe, mojom::SearchResultDefaultRank::kMedium,
      mojom::kManageInputMethodsSubpagePath);

  // Smart inputs.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_SUGGESTIONS_TITLE, mojom::Subpage::kSmartInputs,
      mojom::SearchResultIcon::kGlobe, mojom::SearchResultDefaultRank::kMedium,
      mojom::kSmartInputsSubpagePath);
  static constexpr mojom::Setting kSmartInputsFeaturesSettings[] = {
      mojom::Setting::kShowPersonalInformationSuggestions,
      mojom::Setting::kShowEmojiSuggestions,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSmartInputs,
                            kSmartInputsFeaturesSettings, generator);
}

bool LanguagesSection::IsEmojiSuggestionAllowed() const {
  return base::FeatureList::IsEnabled(
             ::chromeos::features::kEmojiSuggestAddition) &&
         pref_service_->GetBoolean(
             ::chromeos::prefs::kEmojiSuggestionEnterpriseAllowed);
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

}  // namespace settings
}  // namespace chromeos
