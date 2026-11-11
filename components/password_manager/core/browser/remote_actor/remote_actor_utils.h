// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_UTILS_H_

#include <optional>
#include <string_view>

#include "url/origin.h"

namespace password_manager {

// Returns the origin of `domain_or_origin` as sent by a remote actor, or
// `std::nullopt` if it does not form a valid HTTP(S) origin.
// `domain_or_origin` may be a bare host ("example.com"), in which case HTTPS is
// assumed, or a full origin ("http://example.com:8080").
std::optional<url::Origin> GetOriginFromDomainOrOrigin(
    std::string_view domain_or_origin);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_UTILS_H_
