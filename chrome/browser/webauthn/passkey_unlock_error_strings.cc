// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_error_strings.h"

#include <string>

#include "chrome/grit/generated_resources.h"
#include "device/fido/public/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace webauthn {

std::u16string GetPasskeyErrorProfilePillTitle() {
  switch (device::kPasskeyUnlockErrorUiExperimentArm.Get()) {
    case device::PasskeyUnlockErrorUiExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_UNLOCK);
    case device::PasskeyUnlockErrorUiExperimentArm::kGet:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_GET);
    case device::PasskeyUnlockErrorUiExperimentArm::kVerify:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_VERIFY);
  }
}

std::u16string GetPasskeyErrorProfileMenuDetails() {
  switch (device::kPasskeyUnlockErrorUiExperimentArm.Get()) {
    case device::PasskeyUnlockErrorUiExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_UNLOCK);
    case device::PasskeyUnlockErrorUiExperimentArm::kGet:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_GET);
    case device::PasskeyUnlockErrorUiExperimentArm::kVerify:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_VERIFY);
  }
}

std::u16string GetPasskeyErrorProfileMenuButtonLabel() {
  switch (device::kPasskeyUnlockErrorUiExperimentArm.Get()) {
    case device::PasskeyUnlockErrorUiExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_UNLOCK);
    case device::PasskeyUnlockErrorUiExperimentArm::kGet:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_GET);
    case device::PasskeyUnlockErrorUiExperimentArm::kVerify:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_VERIFY);
  }
}

}  // namespace webauthn
