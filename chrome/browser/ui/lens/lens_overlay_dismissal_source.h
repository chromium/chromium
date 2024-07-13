// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_DISMISSAL_SOURCE_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_DISMISSAL_SOURCE_H_

namespace lens {

// Designates the source of any lens overlay dismissal (in other words, any
// call to `LensOverlayController:CloseUI()`).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlayDismissalSource)
enum class LensOverlayDismissalSource {
  // The overlay close button (shown when in the kOverlay state).
  kOverlayCloseButton = 0,

  // A click on the background scrim (shown when in the kOverlayAndResults
  // state).
  kOverlayBackgroundClick = 1,

  // The close button in the side panel.
  kSidePanelCloseButton = 2,

  // The pinned toolbar action button.
  kToolbar = 3,

  // The page in the primary web contents changed (link clicked, back button,
  // etc.).
  kPageChanged = 4,

  // The contents of the associated tab were in the background and discarded
  // to save memory.
  kTabContentsDiscarded = 5,

  // The current tab was backgrounded before the screenshot was created.
  kTabBackgroundedWhileScreenshotting = 6,

  // Creating a screenshot from the view of the web contents failed.
  kErrorScreenshotCreationFailed = 7,

  // Encoding the screenshot failed.
  kErrorScreenshotEncodingFailed = 8,

  // User pressed the escape key.
  kEscapeKeyPress = 9,

  // Another side panel opened forcing our overlay to close.
  kUnexpectedSidePanelOpen = 10,

  // The browser entered fullscreen.
  kFullscreened = 11,

  // The tab was dragged into a new window.
  kTabDragNewWindow = 12,

  // The tab was closed.
  kTabClosed = 13,

  // Renderer closed unexpected (ex. renderer crashed).
  kRendererClosed = 14,

  // The user started finding text on the page underneath.
  kFindInPageInvoked = 15,

  kMaxValue = kFindInPageInvoked
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayDismissalSource)

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_DISMISSAL_SOURCE_H_
