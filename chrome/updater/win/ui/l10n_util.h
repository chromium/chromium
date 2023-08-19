// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_L10N_UTIL_H_
#define CHROME_UPDATER_WIN_UI_L10N_UTIL_H_

#include <windows.h>

#include <string>

using UINT = unsigned int;

namespace updater {

// Returns the preferred language of the updater.
std::wstring GetPreferredLanguage();

// Given the base message id of a string, return the localized string based on
// the language tag. The string is read from the binary's string table, and the
// localized message id of the string will be calculated based off of the
// language offsets defined in updater_installer_strings.h.
std::wstring GetLocalizedString(UINT base_message_id);

// Similar to GetLocalizedString, but preferring `lang` if usable.
std::wstring GetLocalizedString(UINT base_message_id, const std::wstring& lang);

// Returns a formatted version of the localized string in which there is only
// one replacement.
std::wstring GetLocalizedStringF(UINT base_message_id,
                                 const std::wstring& replacement);
// Multivariatic version of GetLocalizedStringF, which can format multiple
// arguments.
std::wstring GetLocalizedStringF(UINT base_message_id,
                                 std::vector<std::wstring> replacements);

// Returns a localized version of the error message associated with an exit
// code.
std::wstring GetLocalizedErrorString(DWORD exit_code);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_L10N_UTIL_H_
