// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_ACTOR_LOGIN_PERMISSION_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_ACTOR_LOGIN_PERMISSION_H_

#include <string>
#include <tuple>

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "url/gurl.h"

namespace password_manager {

// Defines a credential with a permission for Actor Login.
struct ActorLoginPermission {
  // Contains formatted URL or android app name as well as the unformatted
  // version.
  CredentialUIEntry::DomainInfo domain_info;

  // The username of the credential with the permission. If there are
  // multiple credentials with this username and url, all of them are
  // considered to have the permission.
  std::u16string username;

  // URL that allows downloading favicon.
  GURL favicon_url;

  friend bool operator==(const ActorLoginPermission& lhs,
                         const ActorLoginPermission& rhs) = default;

  friend auto operator<=>(const ActorLoginPermission& lhs,
                          const ActorLoginPermission& rhs) = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_ACTOR_LOGIN_PERMISSION_H_
