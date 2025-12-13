// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SCREEN_SHARING_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SCREEN_SHARING_UTIL_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/desktop_media_id.h"

// This enum is used to record UMA histogram metrics for the user's interaction
// with screen-sharing controls exposed by the browser.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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

  base::WeakPtr<ScreensharingControlsHistogramLogger> GetWeakPtr();

 private:
  const content::DesktopMediaID::Type captured_surface_type_;

  bool interaction_with_controls_logged_ = false;

  base::WeakPtrFactory<ScreensharingControlsHistogramLogger> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SCREEN_SHARING_UTIL_H_
