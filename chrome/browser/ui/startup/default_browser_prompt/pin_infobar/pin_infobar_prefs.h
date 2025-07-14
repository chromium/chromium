// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_PREFS_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_PREFS_H_

namespace default_browser {

// The number of days after which to show the infobar again after it's been
// shown once. Exposed for testing.
inline constexpr int kPinInfoBarRepromptDays = 21;

// The maximum number of times the infobar should be shown. Exposed for testing.
inline constexpr int kPinInfoBarMaxPromptCount = 5;

// Records now as the last time the pin-to-taskbar infobar was shown, and
// increments the total number of times shown.
void SetInfoBarShownRecently();

// Returns true if the pin-to-taskbar infobar has been shown within the last
// `kPinInfoBarRepromptDays` days or the maximum total number of times allowed.
bool InfoBarShownRecentlyOrMaxTimes();

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_PREFS_H_
