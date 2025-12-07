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

  // A query was issued from the client to AIM. This is not necessarily the same
  // as the user submitting a query in the composebox, as the query is queued
  // until the handshake is complete.
  kQueryIssued = 1,

  // User submitted a query from the composebox. This differs from kQueryIssued
  // in that the query was submitted by the user, but not yet received by AIM.
  kQuerySubmitted = 2,

  kMaxValue = kQuerySubmitted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensComposeboxUserAction)

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_COMPOSEBOX_USER_ACTION_H_
