// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_INVOCATION_SOURCE_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_INVOCATION_SOURCE_H_

namespace lens {

// Designates the source of any lens overlay invocation (in other words, any
// call to `LensOverlayController::ShowUI()`).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlayInvocationSource)
enum class LensOverlayInvocationSource {
  // The Chrome app ("3-dot") menu entry.
  kAppMenu = 0,

  // The content area context menu entry that is available when the user
  // right-clicks on any area of the page that doesn't contain text, links or
  // media.
  kContentAreaContextMenuPage = 1,

  // The content area context menu entry that is available when the user
  // right-clicks on an image.
  kContentAreaContextMenuImage = 2,

  // The pinned toolbar action button.
  kToolbar = 3,

  // The find in page (Ctrl/Cmd-f) dialog button.
  kFindInPage = 4,

  // The button in the omnibox (address bar).
  kOmnibox = 5,

  kMaxValue = kOmnibox
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:LensOverlayInvocationSource)
// When adding a value here, also update:
// - tools/metrics/histograms/metadata/lens/histogram.xml: <variants
// name="InvocationSources">
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_INVOCATION_SOURCE_H_
