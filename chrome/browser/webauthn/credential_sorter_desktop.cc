// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/credential_sorter_desktop.h"

#include <string>
#include <variant>

#include "base/time/time.h"

using Mechanism = AuthenticatorRequestDialogModel::Mechanism;

namespace webauthn::sorting {

namespace {

struct MechanismTraits {
  static std::u16string GetAccountName(const Mechanism& mechanism) {
    return mechanism.name;
  }

  static SortableCredentialType GetSortableCredentialType(
      const Mechanism& mechanism) {
    if (const auto* cred_variant =
            std::get_if<Mechanism::Credential>(&mechanism.type)) {
      if (cred_variant->value().source == device::AuthenticatorType::kEnclave) {
        return SortableCredentialType::kGpmPasskey;
      }
      // Other AuthenticatorTypes like kTouchID, kWinNative, kICloudKeychain,
      // kPhone are considered Platform Passkeys for the logic.
      return SortableCredentialType::kPlatformPasskey;
    }
    if (std::holds_alternative<Mechanism::Password>(mechanism.type)) {
      return SortableCredentialType::kPassword;
    }
    return SortableCredentialType::kOther;
  }

  static base::Time GetLastUsedTime(const Mechanism& mechanism) {
    if (const auto* password_info =
            std::get_if<Mechanism::Password>(&mechanism.type)) {
      return password_info->value().last_used_time.value_or(base::Time::Min());
    }
    if (const auto* cred_variant =
            std::get_if<Mechanism::Credential>(&mechanism.type)) {
      return cred_variant->value().last_used_time.value_or(base::Time::Min());
    }
    return base::Time::Min();
  }
};

}  // namespace

std::vector<Mechanism> SortMechanisms(std::vector<Mechanism> mechanisms,
                                      UIPresentation ui_presentation) {
  if (ui_presentation != UIPresentation::kModalImmediate) {
    // For non-immediate presentations, return mechanisms as is.
    // Further default sorting could be added here if needed for other modes.
    return mechanisms;
  }

  return ProcessCredentials<Mechanism, MechanismTraits>(std::move(mechanisms));
}

}  // namespace webauthn::sorting
