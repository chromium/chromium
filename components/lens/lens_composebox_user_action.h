// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_COMPOSEBOX_USER_ACTION_H_
#define COMPONENTS_LENS_LENS_COMPOSEBOX_USER_ACTION_H_

namespace lens {

// Enumerates the user interactions with the Lens Composebox in side panel.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensComposeboxUserAction)
enum class LensComposeboxUserAction {
  // User focused into the composebox.
  kFocused = 0,

  // User submitted a query from the composebox.
  kQuerySubmitted = 1,

  kMaxValue = kQuerySubmitted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensComposeboxUserAction)

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_COMPOSEBOX_USER_ACTION_H_
