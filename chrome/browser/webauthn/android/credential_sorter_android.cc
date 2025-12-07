// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/credential_sorter_android.h"

#include <string>
#include <utility>
#include <variant>

#include "base/time/time.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_ui_utils.h"

using password_manager::PasskeyCredential;
using password_manager::UiCredential;

using Credential = TouchToFillView::Credential;

namespace webauthn::sorting {

struct TouchToFillCredentialTraits {
  static std::u16string GetAccountName(const Credential& credential) {
    if (const auto* passkey = std::get_if<PasskeyCredential>(&credential)) {
      return password_manager::ToUsernameString(passkey->username());
    }
    CHECK(std::holds_alternative<UiCredential>(credential));
    return std::get<UiCredential>(credential).username();
  }

  static base::Time GetLastUsedTime(const Credential& credential) {
    if (const auto* passkey = std::get_if<PasskeyCredential>(&credential)) {
      return passkey->last_used_time().value_or(base::Time::Min());
    }
    CHECK(std::holds_alternative<UiCredential>(credential));
    return std::get<UiCredential>(credential).last_used();
  }

  static SortableCredentialType GetSortableCredentialType(
      const Credential& credential) {
    if (std::holds_alternative<PasskeyCredential>(credential)) {
      return SortableCredentialType::kGpmPasskey;
    }
    CHECK(std::holds_alternative<UiCredential>(credential));
    return SortableCredentialType::kPassword;
  }
};

std::vector<Credential> SortTouchToFillCredentials(
    std::vector<Credential> credentials,
    bool immediate_ui_mode) {
  if (!immediate_ui_mode) {
    // For non-immediate presentations, return list as is.
    // Further default sorting could be added here if needed for other modes.
    return credentials;
  }

  return ProcessCredentials<Credential, TouchToFillCredentialTraits>(
      std::move(credentials));
}

}  // namespace webauthn::sorting
