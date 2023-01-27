// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_ENUMS_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_ENUMS_H_

// Button variants for adaptive toolbar button.
// Values must be numbered from 0 and can't have gaps.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.toolbar.adaptive
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
  kMaxValue = kReaderMode,
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_ENUMS_H_
