// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_

#include <compare>
#include <string>

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

// Simple struct that stores a canonicalized credential. Allows implicit
// constructon from PasswordForm, CredentialUIEntry and LeakCheckCredentail for
// convenience.
struct CanonicalizedCredential {
  CanonicalizedCredential(const PasswordForm& form)  // NOLINT
      : canonicalized_username(CanonicalizeUsername(form.username_value)),
        password(form.password_value) {}

  CanonicalizedCredential(const CredentialUIEntry& credential)  // NOLINT
      : canonicalized_username(CanonicalizeUsername(credential.username)),
        password(credential.password) {}

  CanonicalizedCredential(const LeakCheckCredential& credential)  // NOLINT
      : canonicalized_username(CanonicalizeUsername(credential.username())),
        password(credential.password()) {}

  friend auto operator<=>(const CanonicalizedCredential&,
                          const CanonicalizedCredential&) = default;
  friend bool operator==(const CanonicalizedCredential&,
                         const CanonicalizedCredential&) = default;

  std::u16string canonicalized_username;
  std::u16string password;
};

// Returns whether `url` has valid format (either an HTTP or HTTPS scheme) or
// Android credential.
bool IsValidPasswordURL(const GURL& url);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_
