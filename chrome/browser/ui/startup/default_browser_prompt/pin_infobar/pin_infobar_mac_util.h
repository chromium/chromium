// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_MAC_UTIL_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_MAC_UTIL_H_

namespace default_browser {

// Returns true if Chrome is not in the Dock, false if it is or there was an
// error getting the Dock's state.
bool ShouldOfferToPin();

// Adds Chrome to the Dock so it remains there after closing.
// Equivalent to right-clicking Chrome's Dock icon > Options > Keep in Dock.
void PinChromeToDock();

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_MAC_UTIL_H_
