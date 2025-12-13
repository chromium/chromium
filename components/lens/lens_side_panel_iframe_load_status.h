// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_IFRAME_LOAD_STATUS_H_
#define COMPONENTS_LENS_LENS_OVERLAY_IFRAME_LOAD_STATUS_H_

namespace lens {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LensSidePanelIframeLoadStatus)
enum class IframeLoadStatus {
  kSuccess = 0,
  kFailedConnectionRefused = 1,
  kFailedConnectionReset = 2,
  kFailedConnectionTimedOut = 3,
  kFailedTimedOut = 4,
  kFailedNameNotResolved = 5,
  kFailedOther = 6,
  kMaxValue = kFailedOther,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensSidePanelIframeLoadStatus)

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_IFRAME_LOAD_STATUS_H_
