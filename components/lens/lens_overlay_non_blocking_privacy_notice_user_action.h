// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_NON_BLOCKING_PRIVACY_NOTICE_USER_ACTION_H_
#define COMPONENTS_LENS_LENS_OVERLAY_NON_BLOCKING_PRIVACY_NOTICE_USER_ACTION_H_

namespace lens {

// Enumerates the user interactions accepting the non-blocking privacy notice.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlayNonBlockingPrivacyNoticeUserAction)
enum class LensOverlayNonBlockingPrivacyNoticeUserAction {
  // User performed a Lens interaction.
  kLensInteraction = 0,
  // User focused the composebox. Currently not used as composebox interactions
  // will be recorded as Lens interactions.
  kComposeboxFocused = 1,
  // User closed the overlay without accepting the privacy notice.
  kClosedWithoutAccepting = 2,
  // User accepted the privacy notice.
  kAccepted = 3,
  // User dismissed the privacy notice.
  kDismissed = 4,
  kMaxValue = kDismissed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayNonBlockingPrivacyNoticeUserAction)

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_NON_BLOCKING_PRIVACY_NOTICE_USER_ACTION_H_
