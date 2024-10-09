// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_DISMISSAL_SOURCE_H_
#define COMPONENTS_LENS_LENS_OVERLAY_DISMISSAL_SOURCE_H_

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
  // state). Only used on Desktop.
  kOverlayBackgroundClick = 1,

  // The close button in the side panel. Only used on Desktop.
  kSidePanelCloseButton = 2,

  // The pinned toolbar action button. Only used on Desktop.
  kToolbar = 3,

  // The page in the primary web contents changed (link clicked, back button,
  // etc.). Only used on Desktop.
  kPageChanged = 4,

  // The contents of the associated tab were in the background and discarded
  // to save memory. Only used on Desktop.
  kTabContentsDiscarded = 5,

  // The current tab was backgrounded before the screenshot was created. Only
  // used on Desktop.
  kTabBackgroundedWhileScreenshotting = 6,

  // Creating a screenshot from the view of the web contents failed.
  kErrorScreenshotCreationFailed = 7,

  // Encoding the screenshot failed. Only used on Desktop.
  kErrorScreenshotEncodingFailed = 8,

  // User pressed the escape key. Only used on Desktop.
  kEscapeKeyPress = 9,

  // Another side panel opened forcing our overlay to close. Only used on
  // Desktop.
  kUnexpectedSidePanelOpen = 10,

  // The browser entered fullscreen. Only used on Desktop.
  kFullscreened = 11,

  // The tab was dragged into a new window. Only used on Desktop.
  kTabDragNewWindow = 12,

  // The tab was closed.
  kTabClosed = 13,

  // Obsolete: Renderer closed unexpectedly (ex. renderer crashed).
  // Unused, replaced by the kOverlayRendererClosed* and
  // kPageRendererClosed* values below. Only used on Desktop.
  kRendererClosedObsolete = 14,

  // The user started finding text on the page underneath. Only used on Desktop.
  kFindInPageInvoked = 15,

  // The user clicked exit on the preselection toast. Only used on Desktop.
  kPreselectionToastExitButton = 16,

  // The user opened a new side panel entry that replaced the
  // Lens overlay. Only used on Desktop.
  kSidePanelEntryReplaced = 17,

  // The close button in the search bubble. Only used on Desktop.
  kSearchBubbleCloseButton = 18,

  // The overlay's renderer process closed normally. Only used on Desktop.
  kOverlayRendererClosedNormally = 19,

  // The overlay's renderer process closed due to some error. Only used on
  // Desktop.
  kOverlayRendererClosedUnexpectedly = 20,

  // The underlying page's renderer process closed normally. Only used on
  // Desktop.
  kPageRendererClosedNormally = 21,

  // The underlying page's renderer process closed due to some error. Only used
  // on Desktop.
  kPageRendererClosedUnexpectedly = 22,

  // The new default search engine doesn't support Lens. Only used on iOS.
  kDefaultSearchEngineChange = 23,

  // The bottom sheet (iOS) has been dismissed. Only used on iOS.
  kBottomSheetDismissed = 24,

  // Close with the accessibility gesture. Only used on iOS.
  kAccessibilityEscapeGesture = 25,

  // New Lens overlay invocation in another tab. (iOS only support once instance
  // of Lens overlay). Only used on iOS.
  kNewLensInvocation = 26,

  // Lens permissions have been denied. Only used on iOS.
  kLensPermissionsDenied = 27,

  // Lens overlay closed due to low memory warning. Only used on iOS.
  kLowMemory = 28,

  // Lens overlay closed due to network issues. Only used on iOS. (the Lens UI
  // becomes unresponsive with slow connection).
  kNetworkIssue = 29,

  kMaxValue = kNetworkIssue
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayDismissalSource)

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_DISMISSAL_SOURCE_H_
