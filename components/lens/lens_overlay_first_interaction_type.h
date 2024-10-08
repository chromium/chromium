// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_FIRST_INTERACTION_TYPE_H_
#define COMPONENTS_LENS_LENS_OVERLAY_FIRST_INTERACTION_TYPE_H_

namespace lens {

// Designates the type of first lens overlay interaction.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlayFirstInteractionType)
enum class LensOverlayFirstInteractionType {
  // Closing the lens overlay. Only used by iOS.
  kClose = 0,
  // Lens overlay (3-dots) menu.  Only used by iOS.
  kLensMenu = 1,
  // Region selection.
  kRegionSelect = 2,
  // Text selection.
  kTextSelect = 3,
  // Searchbox.  Only used by Desktop.
  kSearchbox = 4,
  // Interact with permission dialog.  Only used by iOS.
  kPermissionDialog = 5,

  kMaxValue = kPermissionDialog
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayFirstInteractionType)
// When adding a value here, also update:
// - tools/metrics/histograms/metadata/lens/histogram.xml: <variants
// name="FirstInteractionType">

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_FIRST_INTERACTION_TYPE_H_
