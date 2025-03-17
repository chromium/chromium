// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_CONSENT_STATUS_H_
#define CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_CONSENT_STATUS_H_

#include "base/component_export.h"

namespace chromeos::editor_menu {

// Defines the status of the consent which we ask the user to provide before
// we can display the feature to them.
// Only append new entries to the end of the enum value list and do not reorder
// the enum value list to maintain compatibility with the integer values saved
// in the pref storage.
enum class EditorConsentStatus : int {
  // User has agreed to consent by pressing "Yes/Agree" button to all dialogs
  // from the consent window.
  kApproved = 0,
  // User has disagreed to consent by pressing "No/Disagree" button to any
  // dialog from the consent window.
  kDeclined = 1,
  // Invalid state of the consent result.
  kInvalid = 2,
  // No explicit consent to use the feature has been received yet.
  kPending = 3,
  // No request has been sent to users to collect their consent.
  kUnset = 4,
};

EditorConsentStatus COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP)
    GetConsentStatusFromInteger(int status_value);

}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_CONSENT_STATUS_H_
