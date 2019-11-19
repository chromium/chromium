// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_CREDENTIAL_MANAGER_TYPES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_CREDENTIAL_MANAGER_TYPES_H_

#include <stddef.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

// Limit the size of the federations array that we pass to the browser to
// something reasonably sane.
const size_t kMaxFederations = 50u;

enum class CredentialType {
  CREDENTIAL_TYPE_EMPTY = 0,
  CREDENTIAL_TYPE_PASSWORD,
  CREDENTIAL_TYPE_FEDERATED,
  CREDENTIAL_TYPE_LAST = CREDENTIAL_TYPE_FEDERATED
};

enum class CredentialManagerError {
  SUCCESS,
  PENDING_REQUEST,
  PASSWORDSTOREUNAVAILABLE,
  UNKNOWN,
};

enum class CredentialMediationRequirement { kSilent, kOptional, kRequired };

std::string CredentialTypeToString(CredentialType value);
std::ostream& operator<<(std::ostream& os, CredentialType value);

struct CredentialInfo {
  CredentialInfo();
  CredentialInfo(const autofill::PasswordForm& form, CredentialType form_type);
  CredentialInfo(const CredentialInfo& other);
  ~CredentialInfo();

  bool operator==(const CredentialInfo& rhs) const;

  CredentialType type;

  // An identifier (username, email address, etc). Corresponds to
  // WebCredential's id property.
  base::Optional<base::string16> id;

  // An user-friendly name ("Jane Doe"). Corresponds to WebCredential's name
  // property.
  base::Optional<base::string16> name;

  // The address of this credential's icon (e.g. the user's avatar).
  // Corresponds to WebCredential's icon property.
  GURL icon;

  // Corresponds to WebPasswordCredential's password property.
  base::Optional<base::string16> password;

  // Corresponds to WebFederatedCredential's provider property.
  url::Origin federation;
};

// Create a new autofill::PasswordForm object based on |info|, valid in the
// context of |origin|. Returns an empty std::unique_ptr for
// CREDENTIAL_TYPE_EMPTY.
std::unique_ptr<autofill::PasswordForm> CreatePasswordFormFromCredentialInfo(
    const CredentialInfo& info,
    const GURL& origin);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_CREDENTIAL_MANAGER_TYPES_H_
