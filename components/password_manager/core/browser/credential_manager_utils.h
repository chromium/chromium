// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_UTILS_H_

#include <memory>

namespace url {
class Origin;
}  // namespace url

namespace password_manager {

struct CredentialInfo;
struct PasswordForm;

// Create a new PasswordForm object based on |info|, valid in the
// context of |origin|. Returns an empty std::unique_ptr for
// CREDENTIAL_TYPE_EMPTY.
std::unique_ptr<PasswordForm> CreatePasswordFormFromCredentialInfo(
    const CredentialInfo& info,
    const url::Origin& origin);

// Creates a CredentialInfo object from `form`.
CredentialInfo PasswordFormToCredentialInfo(const PasswordForm& form);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_UTILS_H_
