// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_DIGEST_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_DIGEST_H_

#include <string>

#include "components/password_manager/core/browser/password_form.h"
#include "url/gurl.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace password_manager {

// Represents a subset of PasswordForm needed for credential retrievals.
struct PasswordFormDigest {
  PasswordFormDigest(PasswordForm::Scheme scheme,
                     const std::string& signon_realm,
                     const GURL& url);
  explicit PasswordFormDigest(const PasswordForm& form);
  explicit PasswordFormDigest(const autofill::FormData& form);
  PasswordFormDigest(const PasswordFormDigest& other);
  PasswordFormDigest(PasswordFormDigest&& other);
  PasswordFormDigest& operator=(const PasswordFormDigest& other);
  PasswordFormDigest& operator=(PasswordFormDigest&& other);

  friend bool operator==(const PasswordFormDigest&,
                         const PasswordFormDigest&) = default;

  PasswordForm::Scheme scheme;
  std::string signon_realm;
  GURL url;
};

// For testing only.
#if defined(UNIT_TEST)
inline std::ostream& operator<<(std::ostream& os,
                                const PasswordFormDigest& digest) {
  return os << "PasswordFormDigest(scheme: " << digest.scheme
            << ", signon_realm: " << digest.signon_realm
            << ", url: " << digest.url << ")";
}
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_DIGEST_H_
