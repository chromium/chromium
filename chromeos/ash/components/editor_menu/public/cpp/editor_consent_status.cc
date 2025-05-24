// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"

#include "base/logging.h"
#include "base/types/cxx23_to_underlying.h"

namespace chromeos::editor_menu {

EditorConsentStatus GetConsentStatusFromInteger(int status_value) {
  switch (status_value) {
    case base::to_underlying(EditorConsentStatus::kUnset):
      return EditorConsentStatus::kUnset;
    case base::to_underlying(EditorConsentStatus::kApproved):
      return EditorConsentStatus::kApproved;
    case base::to_underlying(EditorConsentStatus::kDeclined):
      return EditorConsentStatus::kDeclined;
    case base::to_underlying(EditorConsentStatus::kPending):
      return EditorConsentStatus::kPending;
    default:
      LOG(ERROR) << "Invalid consent status: " << status_value;
      // For any of the invalid states, treat the consent status as unset.
      return EditorConsentStatus::kUnset;
  }
}

}  // namespace chromeos::editor_menu
