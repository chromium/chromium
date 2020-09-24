// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_

#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/strings/string16.h"
#include "base/template_util.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

// Simple struct that stores a canonicalized credential. Allows implicit
// constructon from PasswordForm and LeakCheckCredentail for convenience.
struct CanonicalizedCredential {
  CanonicalizedCredential(const PasswordForm& form)
      : canonicalized_username(CanonicalizeUsername(form.username_value)),
        password(form.password_value) {}

  CanonicalizedCredential(const LeakCheckCredential& credential)
      : canonicalized_username(CanonicalizeUsername(credential.username())),
        password(credential.password()) {}

  base::string16 canonicalized_username;
  base::string16 password;
};

inline bool operator<(const CanonicalizedCredential& lhs,
                      const CanonicalizedCredential& rhs) {
  return std::tie(lhs.canonicalized_username, lhs.password) <
         std::tie(rhs.canonicalized_username, rhs.password);
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_
