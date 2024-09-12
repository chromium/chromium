// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_TEST_UTIL_H_

#include <string_view>
#include <vector>

namespace ash {
class BirchChipButtonBase;

// Ensures the item remover is initialized, otherwise data fetches won't
// complete.
void EnsureItemRemoverInitialized();

// Returns the button from the birch chip bar. Asserts there is only one button.
BirchChipButtonBase* GetBirchChipButton();

// Disables all data type prefs except the given exceptions.
void DisableAllDataTypePrefsExcept(std::vector<std::string_view> exceptions);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_TEST_UTIL_H_
