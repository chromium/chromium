// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_PERMISSION_USER_ACTION_H_
#define COMPONENTS_LENS_LENS_PERMISSION_USER_ACTION_H_

namespace lens {

// Enumerates the user interactions with the Lens Permission Bubble/Dialog
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensPermissionUserAction)
enum class LensPermissionUserAction {
  // User opened the Help Center link.
  kLinkOpened = 0,
  // User pressed the Accept button.
  kAcceptButtonPressed = 1,
  // User pressed the Cancel button.
  kCancelButtonPressed = 2,
  // User pressed the Esc key.
  kEscKeyPressed = 3,
  kMaxValue = kEscKeyPressed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensPermissionBubbleUserAction)

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_PERMISSION_USER_ACTION_H_
