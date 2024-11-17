// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_NEW_TAB_SOURCE_H_
#define COMPONENTS_LENS_LENS_OVERLAY_NEW_TAB_SOURCE_H_

namespace lens {

// Designates the source that opened the new tab in the lens overlay.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlayNewTabSource)
enum class LensOverlayNewTabSource {
  // New tab opened from ...

  // ... navigation to a non-google URL.
  kWebNavigation,
  // ... a unimodal (text-only) omnibox navigation.
  kOmnibox,
  // ... the web context menu.
  kContextMenu,
  kMaxValue = kContextMenu
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayNewTabSource)

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_NEW_TAB_SOURCE_H_
