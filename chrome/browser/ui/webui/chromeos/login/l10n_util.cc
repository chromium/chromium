// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"

#include <stddef.h>

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

const char kMostRelevantLanguagesDivider[] = "MOST_RELEVANT_LANGUAGES_DIVIDER";

namespace {

std::unique_ptr<base::DictionaryValue> CreateInputMethodsEntry(
    const input_method::InputMethodDescriptor& method,
    const std::string selected) {
  input_method::InputMethodUtil* util =
      input_method::InputMethodManager::Get()->GetInputMethodUtil();
  const std::string& ime_id = method.id();
  std::unique_ptr<base::DictionaryValue> input_method(
      new base::DictionaryValue);
  input_method->SetString("value", ime_id);
  input_method->SetString(
      "title", util->GetInputMethodLongNameStripped(method));
  input_method->SetBoolean("selected", ime_id == selected);
  return input_method;
}

// Returns true if element was inserted.
bool InsertString(const std::string& str, std::set<std::string>* to) {
  const std::pair<std::set<std::string>::iterator, bool> result =
      to->insert(str);
  return result.second;
}

void AddOptgroupOtherLayouts(base::ListValue* input_methods_list) {
  std::unique_ptr<base::DictionaryValue> optgroup(new base::DictionaryValue);
  optgroup->SetString(
      "optionGroupName",
      l10n_util::GetStringUTF16(IDS_OOBE_OTHER_KEYBOARD_LAYOUTS));
  input_methods_list->Append(std::move(optgroup));
}

std::unique_ptr<base::DictionaryValue> CreateLanguageEntry(
    const std::string& language_code,
    const base::string16& language_display_name,
    const base::string16& language_native_display_name) {
  base::string16 display_name = language_display_name;
  const bool markup_removal =
      base::i18n::UnadjustStringForLocaleDirection(&display_name);
  DCHECK(markup_removal);

  const bool has_rtl_chars =
      base::i18n::StringContainsStrongRTLChars(display_name);
  const std::string directionality = has_rtl_chars ? "rtl" : "ltr";

  auto dictionary = std::make_unique<base::DictionaryValue>();
  dictionary->SetString("code", language_code);
  dictionary->SetString("displayName", language_display_name);
  dictionary->SetString("textDirection", directionality);
  dictionary->SetString("nativeDisplayName", language_native_display_name);
  return dictionary;
}

// Gets the list of languages with |descriptors| based on |base_language_codes|.
// The |most_relevant_language_codes| will be first in the list. If
// |insert_divider| is true, an entry with its "code" attribute set to
// kMostRelevantLanguagesDivider is placed between the most relevant languages
// and all others.
std::unique_ptr<base::ListValue> GetLanguageList(
    const input_method::InputMethodDescriptors& descriptors,
    const std::vector<std::string>& base_language_codes,
    const std::vector<std::string>& most_relevant_language_codes,
    bool insert_divider) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();

  std::set<std::string> language_codes;
  // Collect the language codes from the supported input methods.
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    const std::vector<std::string>& languages = descriptor.language_codes();
    for (size_t i = 0; i < languages.size(); ++i)
      language_codes.insert(languages[i]);
  }

  // Language sort order.
  std::map<std::string, int /* index */> language_index;
  for (size_t i = 0; i < most_relevant_language_codes.size(); ++i)
    language_index[most_relevant_language_codes[i]] = i;

  // Map of display name -> {language code, native_display_name}.
  // In theory, we should be able to create a map that is sorted by
  // display names using ICU comparator, but doing it is hard, thus we'll
  // use an auxiliary vector to achieve the same result.
  typedef std::pair<std::string, base::string16> LanguagePair;
  typedef std::map<base::string16, LanguagePair> LanguageMap;
  LanguageMap language_map;

  // The auxiliary vector mentioned above (except the most relevant locales).
  std::vector<base::string16> display_names;

  // Separate vector of the most relevant locales.
  std::vector<base::string16> most_relevant_locales_display_names(
      most_relevant_language_codes.size());

  size_t most_relevant_locales_count = 0;

  // Build the list of display names, and build the language map.

  // The list of configured locales might have entries not in
  // base_language_codes. If there are unsupported language variants,
  // but they resolve to backup locale within base_language_codes, also
  // add them to the list.
  for (std::map<std::string, int>::const_iterator it = language_index.begin();
       it != language_index.end(); ++it) {
    const std::string& language_id = it->first;

    const std::string lang = l10n_util::GetLanguage(language_id);

    // Ignore non-specific codes.
    if (lang.empty() || lang == language_id)
      continue;

    if (base::Contains(base_language_codes, language_id)) {
      // Language is supported. No need to replace
      continue;
    }
    std::string resolved_locale;
    if (!l10n_util::CheckAndResolveLocale(language_id, &resolved_locale))
      continue;

    if (!base::Contains(base_language_codes, resolved_locale)) {
      // Resolved locale is not supported.
      continue;
    }

    const base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(language_id, app_locale, true);
    const base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(
            language_id, language_id, true);

    language_map[display_name] =
        std::make_pair(language_id, native_display_name);

    most_relevant_locales_display_names[it->second] = display_name;
    ++most_relevant_locales_count;
  }

  // Translate language codes, generated from input methods.
  for (std::set<std::string>::const_iterator it = language_codes.begin();
       it != language_codes.end(); ++it) {
     // Exclude the language which is not in |base_langauge_codes| even it has
     // input methods.
     if (!base::Contains(base_language_codes, *it))
       continue;

     const base::string16 display_name =
         l10n_util::GetDisplayNameForLocale(*it, app_locale, true);
     const base::string16 native_display_name =
         l10n_util::GetDisplayNameForLocale(*it, *it, true);

     language_map[display_name] = std::make_pair(*it, native_display_name);

     const std::map<std::string, int>::const_iterator index_pos =
         language_index.find(*it);
     if (index_pos != language_index.end()) {
       base::string16& stored_display_name =
           most_relevant_locales_display_names[index_pos->second];
       if (stored_display_name.empty()) {
         stored_display_name = display_name;
         ++most_relevant_locales_count;
       }
    } else {
      display_names.push_back(display_name);
    }
  }
  DCHECK_EQ(display_names.size() + most_relevant_locales_count,
            language_map.size());

  // Build the list of display names, and build the language map.
  for (size_t i = 0; i < base_language_codes.size(); ++i) {
    // Skip this language if it was already added.
    if (language_codes.find(base_language_codes[i]) != language_codes.end())
      continue;

    base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(
            base_language_codes[i], app_locale, false);
    base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(
            base_language_codes[i], base_language_codes[i], false);
    language_map[display_name] =
        std::make_pair(base_language_codes[i], native_display_name);

    const std::map<std::string, int>::const_iterator index_pos =
        language_index.find(base_language_codes[i]);
    if (index_pos != language_index.end()) {
      most_relevant_locales_display_names[index_pos->second] = display_name;
      ++most_relevant_locales_count;
    } else {
      display_names.push_back(display_name);
    }
  }

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings16(app_locale, &display_names);
  // Concatenate most_relevant_locales_display_names and display_names.
  // Insert special divider in between.
  std::vector<base::string16> out_display_names;
  for (size_t i = 0; i < most_relevant_locales_display_names.size(); ++i) {
    if (most_relevant_locales_display_names[i].size() == 0)
      continue;
    out_display_names.push_back(most_relevant_locales_display_names[i]);
  }

  base::string16 divider16;
  if (insert_divider && !out_display_names.empty()) {
    // Insert a divider if requested, but only if
    // |most_relevant_locales_display_names| is not empty.
    divider16 = base::ASCIIToUTF16(kMostRelevantLanguagesDivider);
    out_display_names.push_back(divider16);
  }

  std::copy(display_names.begin(),
            display_names.end(),
            std::back_inserter(out_display_names));

  // Build the language list from the language map.
  std::unique_ptr<base::ListValue> language_list(new base::ListValue());
  for (size_t i = 0; i < out_display_names.size(); ++i) {
    // Sets the directionality of the display language name.
    base::string16 display_name(out_display_names[i]);
    if (insert_divider && display_name == divider16) {
      // Insert divider.
      auto dictionary = std::make_unique<base::DictionaryValue>();
      dictionary->SetString("code", kMostRelevantLanguagesDivider);
      language_list->Append(std::move(dictionary));
      continue;
    }

    const LanguagePair& pair = language_map[out_display_names[i]];
    language_list->Append(
        CreateLanguageEntry(pair.first, out_display_names[i], pair.second));
  }

  return language_list;
}

// Note: this method updates |selected_locale| only if it is empty.
void GetAndMergeKeyboardLayoutsForLocale(input_method::InputMethodUtil* util,
                                         const std::string& locale,
                                         std::string* selected_locale,
                                         std::vector<std::string>* layouts) {
  std::vector<std::string> layouts_from_locale;
  util->GetInputMethodIdsFromLanguageCode(
      locale, input_method::kKeyboardLayoutsOnly, &layouts_from_locale);
  layouts->insert(layouts->end(), layouts_from_locale.begin(),
                  layouts_from_locale.end());
  if (selected_locale->empty() && !layouts_from_locale.empty()) {
    *selected_locale =
        util->GetInputMethodDescriptorFromId(layouts_from_locale[0])->id();
  }
}

// Invokes |callback| with a list of keyboard layouts that can be used for
// |resolved_locale|.
void GetKeyboardLayoutsForResolvedLocale(
    const std::string& requested_locale,
    const GetKeyboardLayoutsForLocaleCallback& callback,
    const std::string& resolved_locale) {
  input_method::InputMethodUtil* util =
      input_method::InputMethodManager::Get()->GetInputMethodUtil();
  std::vector<std::string> layouts = util->GetHardwareInputMethodIds();

  // "Selected" will be set from the fist non-empty list.
  std::string selected;
  GetAndMergeKeyboardLayoutsForLocale(util, requested_locale, &selected,
                                      &layouts);
  GetAndMergeKeyboardLayoutsForLocale(util, resolved_locale, &selected,
                                      &layouts);

  std::unique_ptr<base::ListValue> input_methods_list(new base::ListValue);
  std::set<std::string> input_methods_added;
  for (std::vector<std::string>::const_iterator it = layouts.begin();
       it != layouts.end(); ++it) {
    const input_method::InputMethodDescriptor* ime =
        util->GetInputMethodDescriptorFromId(*it);
    if (!InsertString(ime->id(), &input_methods_added))
      continue;
    input_methods_list->Append(CreateInputMethodsEntry(*ime, selected));
  }

  callback.Run(std::move(input_methods_list));
}

// For "UI Language" drop-down menu at OOBE screen we need to decide which
// entry to mark "selected". If user has just selected "requested_locale",
// but "loaded_locale" was actually loaded, we mark original user choice
// "selected" only if loaded_locale is a backup for "requested_locale".
std::string CalculateSelectedLanguage(const std::string& requested_locale,
                                      const std::string& loaded_locale) {
  std::string resolved_locale;
  if (!l10n_util::CheckAndResolveLocale(requested_locale, &resolved_locale))
    return loaded_locale;

  if (resolved_locale == loaded_locale)
    return requested_locale;

  return loaded_locale;
}

void ResolveLanguageListInThreadPool(
    std::unique_ptr<chromeos::locale_util::LanguageSwitchResult>
        language_switch_result,
    const scoped_refptr<base::TaskRunner> task_runner,
    const UILanguageListResolvedCallback& resolved_callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string selected_language;
  if (!language_switch_result) {
    if (!g_browser_process->GetApplicationLocale().empty()) {
      selected_language = g_browser_process->GetApplicationLocale();
    } else {
      selected_language =
          StartupCustomizationDocument::GetInstance()->initial_locale_default();
    }
  } else {
    if (language_switch_result->success) {
      if (language_switch_result->requested_locale ==
          language_switch_result->loaded_locale) {
        selected_language = language_switch_result->requested_locale;
      } else {
        selected_language =
            CalculateSelectedLanguage(language_switch_result->requested_locale,
                                      language_switch_result->loaded_locale);
      }
    } else {
      selected_language = language_switch_result->loaded_locale;
    }
  }
  const std::string selected_code =
      selected_language.empty() ? g_browser_process->GetApplicationLocale()
                                : selected_language;

  const std::string list_locale =
      language_switch_result ? language_switch_result->loaded_locale
                             : g_browser_process->GetApplicationLocale();
  std::unique_ptr<base::ListValue> language_list(
      chromeos::GetUILanguageList(nullptr, selected_code));

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(resolved_callback, base::Passed(&language_list),
                                list_locale, selected_language));
}

void AdjustUILanguageList(const std::string& selected,
                          base::ListValue* languages_list) {
  for (size_t i = 0; i < languages_list->GetSize(); ++i) {
    base::DictionaryValue* language_info = NULL;
    if (!languages_list->GetDictionary(i, &language_info))
      NOTREACHED();

    std::string value;
    language_info->GetString("code", &value);
    std::string display_name;
    language_info->GetString("displayName", &display_name);
    std::string native_name;
    language_info->GetString("nativeDisplayName", &native_name);

    // If it's an option group divider, add field name.
    if (value == kMostRelevantLanguagesDivider) {
      language_info->SetString(
          "optionGroupName",
          l10n_util::GetStringUTF16(IDS_OOBE_OTHER_LANGUAGES));
    }
    if (display_name != native_name) {
      display_name = base::StringPrintf("%s - %s",
                                        display_name.c_str(),
                                        native_name.c_str());
    }

    language_info->SetString("value", value);
    language_info->SetString("title", display_name);
    if (value == selected)
      language_info->SetBoolean("selected", true);
  }
}

}  // namespace

void ResolveUILanguageList(
    std::unique_ptr<chromeos::locale_util::LanguageSwitchResult>
        language_switch_result,
    const UILanguageListResolvedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&ResolveLanguageListInThreadPool,
                     base::Passed(&language_switch_result),
                     base::SequencedTaskRunnerHandle::Get(), callback));
}

std::unique_ptr<base::ListValue> GetMinimalUILanguageList() {
  const std::string application_locale =
      g_browser_process->GetApplicationLocale();
  base::string16 language_native_display_name =
      l10n_util::GetDisplayNameForLocale(
          application_locale, application_locale, true);

  std::unique_ptr<base::ListValue> language_list(new base::ListValue());
  language_list->Append(CreateLanguageEntry(application_locale,
                                            language_native_display_name,
                                            language_native_display_name));
  AdjustUILanguageList(std::string(), language_list.get());
  return language_list;
}

std::unique_ptr<base::ListValue> GetUILanguageList(
    const std::vector<std::string>* most_relevant_language_codes,
    const std::string& selected) {
  ComponentExtensionIMEManager* manager =
      input_method::InputMethodManager::Get()
          ->GetComponentExtensionIMEManager();
  input_method::InputMethodDescriptors descriptors =
      manager->GetXkbIMEAsInputMethodDescriptor();
  std::unique_ptr<base::ListValue> languages_list(GetLanguageList(
      descriptors, l10n_util::GetAvailableLocales(),
      most_relevant_language_codes
          ? *most_relevant_language_codes
          : StartupCustomizationDocument::GetInstance()->configured_locales(),
      true));
  AdjustUILanguageList(selected, languages_list.get());
  return languages_list;
}

std::string FindMostRelevantLocale(
    const std::vector<std::string>& most_relevant_language_codes,
    const base::ListValue& available_locales,
    const std::string& fallback_locale) {
  for (std::vector<std::string>::const_iterator most_relevant_it =
          most_relevant_language_codes.begin();
       most_relevant_it != most_relevant_language_codes.end();
       ++most_relevant_it) {
    for (base::ListValue::const_iterator available_it =
             available_locales.begin();
         available_it != available_locales.end(); ++available_it) {
      const base::DictionaryValue* dict;
      std::string available_locale;
      if (!available_it->GetAsDictionary(&dict) ||
          !dict->GetString("value", &available_locale)) {
        NOTREACHED();
        continue;
      }
      if (available_locale == *most_relevant_it)
        return *most_relevant_it;
    }
  }

  return fallback_locale;
}

std::unique_ptr<base::ListValue> GetAcceptLanguageList() {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> accept_language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &accept_language_codes);
  return GetLanguageList(
      *input_method::InputMethodManager::Get()->GetSupportedInputMethods(),
      accept_language_codes,
      StartupCustomizationDocument::GetInstance()->configured_locales(),
      false);
}

std::unique_ptr<base::ListValue> GetAndActivateLoginKeyboardLayouts(
    const std::string& locale,
    const std::string& selected,
    bool activate_keyboards) {
  std::unique_ptr<base::ListValue> input_methods_list(new base::ListValue);
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();

  const std::vector<std::string>& hardware_login_input_methods =
      util->GetHardwareLoginInputMethodIds();

  if (activate_keyboards) {
    DCHECK(
        ProfileHelper::IsSigninProfile(ProfileManager::GetActiveUserProfile()));
    manager->GetActiveIMEState()->EnableLoginLayouts(
        locale, hardware_login_input_methods);
  }

  std::unique_ptr<input_method::InputMethodDescriptors> input_methods(
      manager->GetActiveIMEState()->GetActiveInputMethods());
  std::set<std::string> input_methods_added;

  for (std::vector<std::string>::const_iterator i =
           hardware_login_input_methods.begin();
       i != hardware_login_input_methods.end();
       ++i) {
    const input_method::InputMethodDescriptor* ime =
        util->GetInputMethodDescriptorFromId(*i);
    // Do not crash in case of misconfiguration.
    if (ime) {
      input_methods_added.insert(*i);
      input_methods_list->Append(CreateInputMethodsEntry(*ime, selected));
    } else {
      NOTREACHED();
    }
  }

  bool optgroup_added = false;
  for (size_t i = 0; i < input_methods->size(); ++i) {
    // Makes sure the id is in legacy xkb id format.
    const std::string& ime_id = (*input_methods)[i].id();
    if (!InsertString(ime_id, &input_methods_added))
      continue;
    if (!optgroup_added) {
      optgroup_added = true;
      AddOptgroupOtherLayouts(input_methods_list.get());
    }
    input_methods_list->Append(
        CreateInputMethodsEntry((*input_methods)[i], selected));
  }

  // "xkb:us::eng" should always be in the list of available layouts.
  const std::string us_keyboard_id =
      util->GetFallbackInputMethodDescriptor().id();
  if (input_methods_added.find(us_keyboard_id) == input_methods_added.end()) {
    const input_method::InputMethodDescriptor* us_eng_descriptor =
        util->GetInputMethodDescriptorFromId(us_keyboard_id);
    DCHECK(us_eng_descriptor);
    if (!optgroup_added) {
      optgroup_added = true;
      AddOptgroupOtherLayouts(input_methods_list.get());
    }
    input_methods_list->Append(
        CreateInputMethodsEntry(*us_eng_descriptor, selected));
    manager->GetActiveIMEState()->EnableInputMethod(us_keyboard_id);
  }
  return input_methods_list;
}

void GetKeyboardLayoutsForLocale(
    const GetKeyboardLayoutsForLocaleCallback& callback,
    const std::string& locale) {
  // Resolve |locale| on a background thread, then continue on the current
  // thread.
  std::string (*get_application_locale)(const std::string&, bool) =
      &l10n_util::GetApplicationLocale;
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(get_application_locale, locale,
                     false /* set_icu_locale */),
      base::BindOnce(&GetKeyboardLayoutsForResolvedLocale, locale, callback));
}

std::unique_ptr<base::DictionaryValue> GetCurrentKeyboardLayout() {
  const input_method::InputMethodDescriptor current_input_method =
      input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetCurrentInputMethod();
  return CreateInputMethodsEntry(current_input_method,
                                 current_input_method.id());
}

}  // namespace chromeos
