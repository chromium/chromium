// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_PREFS_H_
#define CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_PREFS_H_

class Profile;

// The number of days after which to show the infobar again after it's shown
// once. Multiplied exponentially every subsequent time it's shown.
// Exposed for testing.
inline constexpr int kPdfInfoBarShowIntervalDays = 10;

// The maximum number of times the PDF infobar should be shown.
// Exposed for testing.
inline constexpr int kPdfInfoBarMaxTimesToShow = 5;

// Records now as the last time the PDF infobar was shown, and increments the
// total number of times shown.
void SetInfoBarShownRecently();

// Returns true if the PDF infobar has been shown recently or the maximum total
// number of times allowed. "Recently" means:
// * within the past 10 days if it's been shown once
// * within the past 20 days if it's been shown twice
// * within the past 40 days if it's been shown three times
// * ...and so on, exponentially increasing.
bool InfoBarShownRecentlyOrMaxTimes();

// Returns true if the setting at `chrome://settings/content/pdfDocuments` is
// set to "Download PDFs", which effectively disables the PDF viewer by making
// Chrome download opened PDFs. Returns false if it's set to "Open in Chrome".
bool IsPdfViewerDisabled(Profile* profile);

// Returns true if the system's default browser is controlled by a policy.
bool IsDefaultBrowserPolicyControlled();

#endif  // CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_PREFS_H_
