// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_L10N_UTIL_H_
#define CHROME_UPDATER_WIN_UI_L10N_UTIL_H_

#include <windows.h>

#include <string>
#include <vector>

namespace updater {

// Returns the preferred language of the updater.
std::wstring GetPreferredLanguage();

// Given the base message id of a string, return the localized string based on
// the language tag. The string is read from the binary's string table, and the
// localized message id of the string will be calculated based off of the
// language offsets defined in updater_installer_strings.h.
std::wstring GetLocalizedString(
    unsigned int base_message_id,
    const std::wstring& lang = GetPreferredLanguage());

// Returns a formatted version of the localized string in which there is only
// one replacement.
std::wstring GetLocalizedStringF(
    unsigned int base_message_id,
    const std::wstring& replacement,
    const std::wstring& lang = GetPreferredLanguage());

// Multivariatic version of GetLocalizedStringF, which can format multiple
// arguments.
std::wstring GetLocalizedStringF(
    unsigned int base_message_id,
    std::vector<std::wstring> replacements,
    const std::wstring& lang = GetPreferredLanguage());

// Returns a localized version of the error message associated with a
// metainstaller `exit_code`/`windows_error`.
std::wstring GetLocalizedMetainstallerErrorString(DWORD exit_code,
                                                  DWORD windows_error);

// Returns a localized string to show in the metainstaller splash screen.
std::wstring GetLocalizedSplashScreenString();

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_L10N_UTIL_H_
