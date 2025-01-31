// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_UI_UTILS_H_
#define CHROME_BROWSER_UI_COMMERCE_UI_UTILS_H_

#include <string>

class Browser;

namespace commerce {

// Shows the "added to set" confirmation toast if the toast feature is enabled.
void ShowProductSpecsConfirmationToast(std::u16string set_name,
                                       Browser* browser);

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_UI_UTILS_H_
