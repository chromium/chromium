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
  ActorLoginPermission(GURL url,
                       std::string human_readable_name,
                       std::string signon_realm,
                       std::u16string username);
  ActorLoginPermission(const ActorLoginPermission& other);
  ~ActorLoginPermission();
  // TODO(crbug.com/462214930): remove once kActorLoginPermissionsForAndroid
  // is cleaned up.
  GURL url;
  // Represents human readable info that can be shown in the UI. This also
  // covers android apps.
  std::string human_readable_name;
  // Url or android app. Comes from PasswordForm::signon_realm
  std::string signon_realm;
  // The username of the credential with the permission. If there are multiple
  // credentials with this username and url, all of them are considered to
  // have the permission.
  std::u16string username;

  auto operator<=>(const ActorLoginPermission& other) const = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_ACTOR_LOGIN_PERMISSION_H_
