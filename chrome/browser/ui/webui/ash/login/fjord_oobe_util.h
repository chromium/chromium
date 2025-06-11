// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_OOBE_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_OOBE_UTIL_H_

#include <set>
#include <string>
#include <string_view>

namespace ash::fjord_util {

// Returns if the Fjord variant of OOBE should be shown.
bool ShouldShowFjordOobe();

// Returns if the language code is allowlisted for Fjord OOBE.
bool IsAllowlistedLanguage(std::string_view language_code);

const std::set<std::string>& GetAllowlistedLanguagesForTesting();

}  // namespace ash::fjord_util

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_OOBE_UTIL_H_
