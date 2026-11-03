// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_ERROR_STRINGS_H_
#define CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_ERROR_STRINGS_H_

#include <string>

namespace webauthn {

// Functions providing the UI strings for the passkey unlock error UI. Results
// depend on the experiment arms configured by the feature parameter
// `device::kPasskeyUnlockErrorUiExperimentArm`.
std::u16string GetPasskeyErrorProfilePillTitle();
std::u16string GetPasskeyErrorProfileMenuDetails();
std::u16string GetPasskeyErrorProfileMenuButtonLabel();

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_ERROR_STRINGS_H_
