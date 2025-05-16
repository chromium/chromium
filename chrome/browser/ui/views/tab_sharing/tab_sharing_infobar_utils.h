// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_UTILS_H_

// This enum is used to record UMA histogram metrics for interactions
// with the TabSharingInfoBar.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep this in sync with MediaUiGetDisplayMediaTabSharingInfoBarInteraction in
// tools/metrics/histograms/metadata/media/enums.xml.
enum class TabSharingInfoBarInteraction {
  kCapturedToCapturing = 0,
  kCapturingToCaptured = 1,
  kOtherToCapturing = 2,
  kOtherToCaptured = 3,
  kStopButtonClicked = 4,
  kShareThisTabInsteadButtonClicked = 5,
  kMaxValue = kShareThisTabInsteadButtonClicked,
};

void RecordUma(TabSharingInfoBarInteraction interaction);

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_UTILS_H_
