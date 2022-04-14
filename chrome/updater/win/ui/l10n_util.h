// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_L10N_UTIL_H_
#define CHROME_UPDATER_WIN_UI_L10N_UTIL_H_

#include <string>

namespace updater {

// Given the base message id of a string, return the localized string based on
// the language tag. The string is read from the binary's string table, and the
// localized message id of the string will be calculated based off of the
// language offsets defined in updater_installer_strings.h.
std::wstring GetLocalizedString(int base_message_id);
// Returns a formatted version of the localized string in which there is only
// one replacement.
std::wstring GetLocalizedStringF(int base_message_id,
                                 const std::wstring& replacement);
// Multivariatic version of GetLocalizedStringF, which can format multiple
// arguments.
std::wstring GetLocalizedStringF(int base_message_id,
                                 std::vector<std::wstring> replacements);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_L10N_UTIL_H_
