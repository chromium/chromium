// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SCREEN_SHARING_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SCREEN_SHARING_UTIL_H_

#include "content/public/browser/desktop_media_id.h"

// This enum is used to record UMA histogram metrics for interactions
// with the TabSharingInfoBar.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Keep this in sync with MediaUiGetDisplayMediaTabSharingInfoBarInteraction in
// tools/metrics/histograms/metadata/media/enums.xml.
//
// TODO(crbug.com/440459628): [Mid-term] Sunset this histogram and use
// GetDisplayMediaUserInteractionWithControls in its stead.
//
// LINT.IfChange(TabSharingInfoBarInteraction)
enum class TabSharingInfoBarInteraction {
  kCapturedToCapturing = 0,
  kCapturingToCaptured = 1,
  kOtherToCapturing = 2,
  kOtherToCaptured = 3,
  kStopButtonClicked = 4,
  kShareThisTabInsteadButtonClicked = 5,
  kMaxValue = kShareThisTabInsteadButtonClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:MediaUiGetDisplayMediaTabSharingInfoBarInteraction)

// TODO(crbug.com/440459628): [Short-term] Roll into the logger class below.
void RecordUma(TabSharingInfoBarInteraction interaction);

// This enum is used to record UMA histogram metrics for the user's interaction
// with screen-sharing controls exposed by the browser.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Keep this in sync with GetDisplayMediaUserInteractionWithControls in
// tools/metrics/histograms/metadata/media/enums.xml.
//
// LINT.IfChange(GetDisplayMediaUserInteractionWithControls)
enum class GetDisplayMediaUserInteractionWithControls {
  kNoInteraction = 0,
  kStopButtonClicked = 1,
  kHideButtonClicked = 2,
  kCapturedToCapturingClicked = 3,
  kCapturingToCapturedClicked = 4,
  kOtherToCapturingClicked = 5,
  kOtherToCapturedClicked = 6,
  kShareThisTabInsteadClicked = 7,
  kMaxValue = kShareThisTabInsteadClicked
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:GetDisplayMediaUserInteractionWithControls)

class ScreensharingControlsHistogramLogger {
 public:
  explicit ScreensharingControlsHistogramLogger(
      content::DesktopMediaID::Type captured_surface_type);
  virtual ~ScreensharingControlsHistogramLogger();

  // Logs Media.Ui.GetDisplayMedia.%s.UserInteractionWithControls.
  void Log(GetDisplayMediaUserInteractionWithControls interaction);

 private:
  const content::DesktopMediaID::Type captured_surface_type_;

  bool interaction_with_controls_logged_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SCREEN_SHARING_UTIL_H_
