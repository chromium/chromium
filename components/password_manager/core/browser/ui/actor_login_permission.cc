// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/actor_login_permission.h"

namespace password_manager {

ActorLoginPermission::ActorLoginPermission(GURL url,
                                           std::string human_readable_name,
                                           std::string signon_realm,
                                           std::u16string username)
    : url(url),
      human_readable_name(human_readable_name),
      signon_realm(signon_realm),
      username(username) {}

ActorLoginPermission::ActorLoginPermission(const ActorLoginPermission& other) =
    default;

ActorLoginPermission::~ActorLoginPermission() = default;

}  // namespace password_manager
