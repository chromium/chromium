// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_

#include <string>
#include <utility>

#include "build/build_config.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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

  CanonicalizedCredential(CredentialUIEntry&& credential)  // NOLINT
      : canonicalized_username(CanonicalizeUsername(credential.username)),
        password(std::move(credential.password)) {}

#if !BUILDFLAG(IS_ANDROID)
  CanonicalizedCredential(const LeakCheckCredential& credential)  // NOLINT
      : canonicalized_username(CanonicalizeUsername(credential.username())),
        // TODO(crbug.com/513276101): Remove the explicit copy constructor call
        // when LeakCheckCredential is converted to store password as
        // PasswordString.
        password(std::u16string(credential.password())) {}
#endif  // !BUILDFLAG(IS_ANDROID)

  friend bool operator==(const CanonicalizedCredential&,
                         const CanonicalizedCredential&) = default;

  template <typename H>
  friend H AbslHashValue(H h, const CanonicalizedCredential& credential) {
    return H::combine(std::move(h), credential.canonicalized_username,
                      credential.password);
  }

  std::u16string canonicalized_username;
  PasswordString password;
};

// Returns whether `url` has valid format (either an HTTP or HTTPS scheme) or
// Android credential.
bool IsValidPasswordURL(const GURL& url);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_
