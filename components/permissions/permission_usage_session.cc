// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_usage_session.h"

#include <tuple>

namespace permissions {

bool PermissionUsageSession::operator==(
    const PermissionUsageSession& other) const {
  return std::tie(origin, type, usage_start, usage_end, had_user_activation,
                  was_foreground, had_focus) ==
         std::tie(other.origin, other.type, other.usage_start, other.usage_end,
                  other.had_user_activation, other.was_foreground,
                  other.had_focus);
}

bool PermissionUsageSession::operator!=(
    const PermissionUsageSession& other) const {
  return !(*this == other);
}

bool PermissionUsageSession::IsValid() const {
  return !(origin.opaque() || usage_start.is_null() || usage_end.is_null() ||
           usage_end < usage_start);
}

}  // namespace permissions
