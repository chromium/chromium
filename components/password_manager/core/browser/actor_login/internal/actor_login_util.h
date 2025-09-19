// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_UTIL_H_

#include <string>

#include "url/gurl.h"

namespace password_manager {
class PasswordFormCache;
class PasswordFormManager;
}  // namespace password_manager

namespace url {
class Origin;
}  // namespace url

namespace actor_login {

// Transforms `url` into `Credential.source_site_or_app`
std::u16string GetSourceSiteOrAppFromUrl(const GURL& url);

// Returns the `PasswordFormManager` for a sign-in form on the page matching the
// given `origin`, or nullptr if no such form exists.
password_manager::PasswordFormManager* GetSigninFormManager(
    const url::Origin& origin,
    password_manager::PasswordFormCache* form_cache);

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_UTIL_H_
