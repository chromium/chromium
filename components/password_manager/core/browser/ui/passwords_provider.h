// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORDS_PROVIDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORDS_PROVIDER_H_

#include "base/containers/flat_set.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {

// Interface for an object holding saved credentials and providing
// them to clients via a getter.
class PasswordsProvider {
 public:
  PasswordsProvider() = default;
  virtual ~PasswordsProvider() = default;

  // Returns a list of unique passwords which includes normal credentials,
  // federated credentials, passkeys, and blocked forms. If a same form is
  // present both on account and profile stores it will be represented as a
  // single entity. Uniqueness is determined using site name, username,
  // password. For Android credentials package name is also taken into account
  // and for Federated credentials federation origin.
  virtual std::vector<CredentialUIEntry> GetSavedCredentials() const = 0;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Returns the set of sites where Actor Login is allowed, deduped by a url +
  // username pair.
  virtual base::flat_set<ActorLoginPermission> GetActorLoginPermissions(
      syncer::SyncService* sync_service) const = 0;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORDS_PROVIDER_H_
