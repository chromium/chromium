// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/l10n_util.h"

#include <stddef.h>

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

const char kMostRelevantLanguagesDivider[] = "MOST_RELEVANT_LANGUAGES_DIVIDER";

namespace {

// The code below needs a UTF16 version of kMostRelevantLanguagesDivider.
// Provide it here as a constant.
constexpr char16_t kMostRelevantLanguagesDivider16[] =
    u"MOST_RELEVANT_LANGUAGES_DIVIDER";

base::Value CreateInputMethodsEntry(
    const input_method::InputMethodDescriptor& method,
    const std::string& selected,
    input_method::InputMethodUtil* util) {
  const std::string& ime_id = method.id();
  auto input_method =
      base::Value::Dict()
          .Set("value", ime_id)
          .Set("title", util->GetInputMethodLongNameStripped(method))
          .Set("selected", ime_id == selected);
  return base::Value(std::move(input_method));
}

// Returns true if element was inserted.
bool InsertString(const std::string& str, std::set<std::string>* to) {
  const std::pair<std::set<std::string>::iterator, bool> result =
      to->insert(str);
  return result.second;
}

void AddOptgroupOtherLayouts(base::Value::List& input_methods_list) {
  // clang-format off
  input_methods_list.Append(base::Value::Dict()
    .Set("optionGroupName",
         l10n_util::GetStringUTF16(IDS_OOBE_OTHER_KEYBOARD_LAYOUTS)));
  // clang-format on
}

base::Value::Dict CreateLanguageEntry(
    const std::string& language_code,
    const std::u16string& language_display_name,
    const std::u16string& language_native_display_name) {
  std::u16string display_name = language_display_name;
  const bool markup_removal =
      base::i18n::UnadjustStringForLocaleDirection(&display_name);
  DCHECK(markup_removal);

  const bool has_rtl_chars =
      base::i18n::StringContainsStrongRTLChars(display_name);
  const char* directionality = has_rtl_chars ? "rtl" : "ltr";

  return base::Value::Dict()
      .Set("code", language_code)
      .Set("displayName", language_display_name)
      .Set("textDirection", directionality)
      .Set("nativeDisplayName", language_native_display_name);
}

// Gets the list of languages with `descriptors` based on `base_language_codes`.
// The `most_relevant_language_codes` will be first in the list. If
// `insert_divider` is true, an entry with its "code" attribute set to
// kMostRelevantLanguagesDivider is placed between the most relevant languages
// and all others.
base::Value::List GetLanguageList(
    const input_method::InputMethodDescriptors& descriptors,
    const std::vector<std::string>& base_language_codes,
    const std::vector<std::string>& most_relevant_language_codes,
    bool insert_divider) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();

  std::set<std::string> language_codes;
  // Collect the language codes from the supported input methods.
  for (const auto& descriptor : descriptors) {
    for (const auto& language : descriptor.language_codes())
      language_codes.insert(language);
  }

  // Language sort order.
  std::map<std::string, int /* index */> language_index;
  for (size_t i = 0; i < most_relevant_language_codes.size(); ++i)
    language_index[most_relevant_language_codes[i]] = i;

  // Map of display name -> {language code, native_display_name}.
  // In theory, we should be able to create a map that is sorted by
  // display names using ICU comparator, but doing it is hard, thus we'll
  // use an auxiliary vector to achieve the same result.
  typedef std::pair<std::string, std::u16string> LanguagePair;
  typedef std::map<std::u16string, LanguagePair> LanguageMap;
  LanguageMap language_map;

  // The auxiliary vector mentioned above (except the most relevant locales).
  std::vector<std::u16string> display_names;

  // Separate vector of the most relevant locales.
  std::vector<std::u16string> most_relevant_locales_display_names(
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

    const std::u16string display_name =
        l10n_util::GetDisplayNameForLocale(language_id, app_locale, true);
    const std::u16string native_display_name =
        l10n_util::GetDisplayNameForLocale(language_id, language_id, true);

    language_map[display_name] =
        std::make_pair(language_id, native_display_name);

    most_relevant_locales_display_names[it->second] = display_name;
    ++most_relevant_locales_count;
  }

  // Translate language codes, generated from input methods.
  for (const auto& language_code : language_codes) {
    // Exclude the language which is not in `base_langauge_codes` even it has
    // input methods.
    if (!base::Contains(base_language_codes, language_code))
      continue;

    const std::u16string display_name =
        l10n_util::GetDisplayNameForLocale(language_code, app_locale, true);
    const std::u16string native_display_name =
        l10n_util::GetDisplayNameForLocale(language_code, language_code, true);

    language_map[display_name] =
        std::make_pair(language_code, native_display_name);

    const std::map<std::string, int>::const_iterator index_pos =
        language_index.find(language_code);
    if (index_pos != language_index.end()) {
      std::u16string& stored_display_name =
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
  for (const auto& base_language_code : base_language_codes) {
    // Skip this language if it was already added.
    if (language_codes.find(base_language_code) != language_codes.end())
      continue;

    std::u16string display_name = l10n_util::GetDisplayNameForLocale(
        base_language_code, app_locale, false);
    std::u16string native_display_name = l10n_util::GetDisplayNameForLocale(
        base_language_code, base_language_code, false);
    language_map[display_name] =
        std::make_pair(base_language_code, native_display_name);

    const std::map<std::string, int>::const_iterator index_pos =
        language_index.find(base_language_code);
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
  std::vector<std::u16string> out_display_names;
  for (const auto& most_relevant_locales_display_name :
       most_relevant_locales_display_names) {
    if (most_relevant_locales_display_name.size() == 0)
      continue;
    out_display_names.push_back(most_relevant_locales_display_name);
  }

  std::u16string divider16;
  if (insert_divider && !out_display_names.empty()) {
    // Insert a divider if requested, but only if
    // `most_relevant_locales_display_names` is not empty.
    divider16 = kMostRelevantLanguagesDivider16;
    out_display_names.push_back(divider16);
  }

  base::ranges::copy(display_names, std::back_inserter(out_display_names));

  // Build the language list from the language map.
  base::Value::List language_list;
  for (const auto& out_display_name : out_display_names) {
    // Sets the directionality of the display language name.
    std::u16string display_name(out_display_name);
    if (insert_divider && display_name == divider16) {
      // Insert divider.
      language_list.Append(
          base::Value::Dict().Set("code", kMostRelevantLanguagesDivider));
      continue;
    }

    const LanguagePair& pair = language_map[out_display_name];
    language_list.Append(
        CreateLanguageEntry(pair.first, out_display_name, pair.second));
  }

  return language_list;
}

// Note: this method updates `selected_locale` only if it is empty.
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

// Invokes `callback` with a list of keyboard layouts that can be used for
// `resolved_locale`.
void GetKeyboardLayoutsForResolvedLocale(
    const std::string& requested_locale,
    input_method::InputMethodManager* input_method_manager,
    GetKeyboardLayoutsForLocaleCallback callback,
    const std::string& resolved_locale) {
  input_method::InputMethodUtil* util =
      input_method_manager->GetInputMethodUtil();
  std::vector<std::string> layouts = util->GetHardwareInputMethodIds();

  // "Selected" will be set from the fist non-empty list.
  std::string selected;
  GetAndMergeKeyboardLayoutsForLocale(util, requested_locale, &selected,
                                      &layouts);
  GetAndMergeKeyboardLayoutsForLocale(util, resolved_locale, &selected,
                                      &layouts);

  base::Value::List input_methods_list;
  std::set<std::string> input_methods_added;
  for (std::vector<std::string>::const_iterator it = layouts.begin();
       it != layouts.end(); ++it) {
    const input_method::InputMethodDescriptor* ime =
        util->GetInputMethodDescriptorFromId(*it);
    if (!InsertString(ime->id(), &input_methods_added))
      continue;
    input_methods_list.Append(CreateInputMethodsEntry(*ime, selected, util));
  }

  std::move(callback).Run(std::move(input_methods_list));
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
    const std::string& locale,
    std::unique_ptr<locale_util::LanguageSwitchResult> language_switch_result,
    const scoped_refptr<base::TaskRunner> task_runner,
    input_method::InputMethodManager* input_method_manager,
    UILanguageListResolvedCallback resolved_callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string selected_language;
  if (language_switch_result) {
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
  } else {
    selected_language = !locale.empty()
                            ? locale
                            : StartupCustomizationDocument::GetInstance()
                                  ->initial_locale_default();
  }
  const std::string selected_code =
      selected_language.empty() ? locale : selected_language;

  const std::string list_locale =
      language_switch_result ? language_switch_result->loaded_locale : locale;
  base::Value::List language_list(
      GetUILanguageList(nullptr, selected_code, input_method_manager));

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(resolved_callback), std::move(language_list),
                     list_locale, selected_language));
}

void AdjustUILanguageList(const std::string& selected,
                          base::Value::List& languages_list) {
  for (auto& it : languages_list) {
    base::Value::Dict& language_info = it.GetDict();

    std::string value = language_info.FindString("code")
                            ? *language_info.FindString("code")
                            : "";
    std::string display_name = language_info.FindString("displayName")
                                   ? *language_info.FindString("displayName")
                                   : "";
    std::string native_name =
        language_info.FindString("nativeDisplayName")
            ? *language_info.FindString("nativeDisplayName")
            : "";

    // If it's an option group divider, add field name.
    if (value == kMostRelevantLanguagesDivider) {
      language_info.Set("optionGroupName",
                        l10n_util::GetStringUTF16(IDS_OOBE_OTHER_LANGUAGES));
    }
    if (display_name != native_name) {
      display_name = base::StringPrintf("%s - %s", display_name.c_str(),
                                        native_name.c_str());
    }

    language_info.Set("value", value);
    language_info.Set("title", display_name);
    if (value == selected)
      language_info.Set("selected", true);
  }
}

}  // namespace

void ResolveUILanguageList(
    std::unique_ptr<locale_util::LanguageSwitchResult> language_switch_result,
    input_method::InputMethodManager* input_method_manager,
    UILanguageListResolvedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ResolveLanguageListInThreadPool,
                     g_browser_process->GetApplicationLocale(),
                     std::move(language_switch_result),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     input_method_manager, std::move(callback)));
}

base::Value::List GetMinimalUILanguageList() {
  const std::string application_locale =
      g_browser_process->GetApplicationLocale();
  std::u16string language_native_display_name =
      l10n_util::GetDisplayNameForLocale(application_locale, application_locale,
                                         true);

  base::Value::List language_list;
  language_list.Append(CreateLanguageEntry(application_locale,
                                           language_native_display_name,
                                           language_native_display_name));
  AdjustUILanguageList(application_locale, language_list);
  return language_list;
}

base::Value::List GetUILanguageList(
    const std::vector<std::string>* most_relevant_language_codes,
    const std::string& selected,
    input_method::InputMethodManager* input_method_manager) {
  ComponentExtensionIMEManager* component_extension_ime_manager =
      input_method_manager->GetComponentExtensionIMEManager();
  input_method::InputMethodDescriptors descriptors =
      component_extension_ime_manager->GetXkbIMEAsInputMethodDescriptor();
  base::Value::List languages_list(GetLanguageList(
      descriptors, l10n_util::GetUserFacingUILocaleList(),
      most_relevant_language_codes
          ? *most_relevant_language_codes
          : StartupCustomizationDocument::GetInstance()->configured_locales(),
      true));
  AdjustUILanguageList(selected, languages_list);
  return languages_list;
}

std::string FindMostRelevantLocale(
    const std::vector<std::string>& most_relevant_language_codes,
    const base::Value::List& available_locales,
    const std::string& fallback_locale) {
  for (const auto& most_relevant : most_relevant_language_codes) {
    for (const auto& entry : available_locales) {
      const std::string* available_locale = nullptr;
      if (entry.is_dict())
        available_locale = entry.GetDict().FindString("value");

      if (!available_locale) {
        NOTREACHED_IN_MIGRATION();
        continue;
      }

      if (*available_locale == most_relevant)
        return most_relevant;
    }
  }

  return fallback_locale;
}

base::Value::List GetAndActivateLoginKeyboardLayouts(
    const std::string& locale,
    const std::string& selected,
    input_method::InputMethodManager* input_method_manager) {
  base::Value::List input_methods_list;
  input_method::InputMethodUtil* util =
      input_method_manager->GetInputMethodUtil();

  const std::vector<std::string>& hardware_login_input_methods =
      util->GetHardwareLoginInputMethodIds();

  DCHECK(
      ProfileHelper::IsSigninProfile(ProfileManager::GetActiveUserProfile()));
  input_method_manager->GetActiveIMEState()->EnableLoginLayouts(
      locale, hardware_login_input_methods);

  input_method::InputMethodDescriptors input_methods(
      input_method_manager->GetActiveIMEState()->GetEnabledInputMethods());
  std::set<std::string> input_methods_added;

  for (const auto& hardware_login_input_method : hardware_login_input_methods) {
    const input_method::InputMethodDescriptor* ime =
        util->GetInputMethodDescriptorFromId(hardware_login_input_method);
    // Do not crash in case of misconfiguration.
    if (ime) {
      input_methods_added.insert(hardware_login_input_method);
      input_methods_list.Append(CreateInputMethodsEntry(*ime, selected, util));
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  bool optgroup_added = false;
  for (size_t i = 0; i < input_methods.size(); ++i) {
    // Makes sure the id is in legacy xkb id format.
    const std::string& ime_id = input_methods[i].id();
    if (!InsertString(ime_id, &input_methods_added))
      continue;
    if (!optgroup_added) {
      optgroup_added = true;
      AddOptgroupOtherLayouts(input_methods_list);
    }
    input_methods_list.Append(
        CreateInputMethodsEntry(input_methods[i], selected, util));
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
      AddOptgroupOtherLayouts(input_methods_list);
    }
    input_methods_list.Append(
        CreateInputMethodsEntry(*us_eng_descriptor, selected, util));
    input_method_manager->GetActiveIMEState()->EnableInputMethod(
        us_keyboard_id);
  }
  return input_methods_list;
}

void GetKeyboardLayoutsForLocale(
    GetKeyboardLayoutsForLocaleCallback callback,
    const std::string& locale,
    input_method::InputMethodManager* input_method_manager) {
  // Resolve `locale` on a background thread, then continue on the current
  // thread.
  std::string (*get_application_locale)(const std::string&, bool) =
      &l10n_util::GetApplicationLocale;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(get_application_locale, locale,
                     false /* set_icu_locale */),
      base::BindOnce(&GetKeyboardLayoutsForResolvedLocale, locale,
                     input_method_manager, std::move(callback)));
}

}  // namespace ash
