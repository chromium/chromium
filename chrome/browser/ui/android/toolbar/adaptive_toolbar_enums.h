// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_ENUMS_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_ENUMS_H_

// Button variants for adaptive toolbar button.
// Values must be numbered from 0 and can't have gaps.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Be sure to keep in sync with AdaptiveToolbarButtonVariant in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.toolbar.adaptive
//
// LINT.IfChange(AdaptiveToolbarButtonVariant)
enum class AdaptiveToolbarButtonVariant {
  // Button type is unknown. Used as default in backend logic.
  kUnknown = 0,
  // Used only in UI layer when we don't want a button to show.
  kNone = 1,
  // Button type is new tab button.
  kNewTab = 2,
  // Share button.
  kShare = 3,
  // Voice button.
  kVoice = 4,
  // Automatic. Used in settings page to indicate that user hasn't manually
  // overridden the button.
  kAuto = 5,
  // Track price action.
  kPriceTracking = 6,
  // Reader mode action.
  kReaderMode = 7,
  // Translate page action.
  kTranslate = 8,
  // Add to bookmarks action.
  kAddToBookmarks = 9,
  // ReadAloud action.
  kReadAloud = 10,
  // Value for testing.
  kTestButton = 11,
  // Price Insights action.
  kPriceInsights = 12,
  // Page Summary button.
  kPageSummary = 13,
  // Discounts action.
  kDiscounts = 14,
  kMaxValue = kDiscounts,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:AdaptiveToolbarButtonVariant)

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_ENUMS_H_
