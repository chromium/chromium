// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_L10N_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_L10N_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

typedef base::OnceCallback<void(
    base::Value::List /* new_language_list */,
    const std::string& /* new_language_list_locale */,
    const std::string& /* new_selected_language */)>
    UILanguageListResolvedCallback;

// GetUILanguageList() returns a concatenated list of the most relevant
// languages followed by all others. An entry with its "code" attribute set to
// this value is inserted in between.
extern const char kMostRelevantLanguagesDivider[];

// Utility methods for retrieving lists of supported locales and input methods /
// keyboard layouts during OOBE and on the login screen.

// Return a list of languages in which the UI can be shown. Each list entry is a
// dictionary that contains data such as the language's locale code and a
// display name. The list will consist of the `most_relevant_language_codes`,
// followed by a divider and all other supported languages after that. If
// `most_relevant_language_codes` is NULL, the most relevant languages are read
// from initial_locale in VPD. If `selected` matches the locale code of any
// entry in the resulting list, that entry will be marked as selected.
base::Value::List GetUILanguageList(
    const std::vector<std::string>* most_relevant_language_codes,
    const std::string& selected,
    input_method::InputMethodManager* input_method_manager);

// Must be called on UI thread. Runs GetUILanguageList(), on Blocking Pool,
// and calls `callback` on UI thread with result.
// If `language_switch_result` is null, assume current browser locale is already
// correct and has been successfully loaded.
void ResolveUILanguageList(
    std::unique_ptr<locale_util::LanguageSwitchResult> language_switch_result,
    input_method::InputMethodManager* input_method_manager,
    UILanguageListResolvedCallback callback);

// Returns a minimal list of UI languages, which consists of active language
// only. It is used as a placeholder until ResolveUILanguageList() finishes
// on BlockingPool.
base::Value::List GetMinimalUILanguageList();

// Returns the most first entry of `most_relevant_language_codes` that is
// actually available (present in `available_locales`). If none of the entries
// are present in `available_locales`, returns the `fallback_locale`.
std::string FindMostRelevantLocale(
    const std::vector<std::string>& most_relevant_language_codes,
    const base::Value::List& available_locales,
    const std::string& fallback_locale);

// Return a list of keyboard layouts that can be used for `locale` on the login
// screen. Each list entry is a dictionary that contains data such as an ID and
// a display name. The list will consist of the device's hardware layouts,
// followed by a divider and locale-specific keyboard layouts, if any. The list
// will also always contain the US keyboard layout. If `selected` matches the ID
// of any entry in the resulting list, that entry will be marked as selected.
// In addition to returning the list of keyboard layouts, this function also
// activates them, so that they can be selected by the user (e.g. by cycling
// through keyboard layouts via keyboard shortcuts).
base::Value::List GetAndActivateLoginKeyboardLayouts(
    const std::string& locale,
    const std::string& selected,
    input_method::InputMethodManager* input_method_manager);

// Invokes `callback` with a list of keyboard layouts that can be used for
// `locale`. Each list entry is a dictionary that contains data such as an ID
// and a display name. The list will consist of the device's hardware layouts,
// followed by a divider and locale-specific keyboard layouts, if any. All
// layouts supported for `locale` are returned, including those that produce
// non-Latin characters by default.
typedef base::OnceCallback<void(base::Value::List)>
    GetKeyboardLayoutsForLocaleCallback;
void GetKeyboardLayoutsForLocale(
    GetKeyboardLayoutsForLocaleCallback callback,
    const std::string& locale,
    input_method::InputMethodManager* input_method_manager);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_L10N_UTIL_H_
