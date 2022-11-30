// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_USAGE_SESSION_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_USAGE_SESSION_H_

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/origin.h"

namespace permissions {

// Stores information about a permission usage session, which is a continuous
// time interval during which some permission was used by some site. For
// instance, a usage session could be a time interval during which a site
// accessed the camera. {type, origin, usage_start} forms the primary of a
// session.
struct PermissionUsageSession {
  //  The `origin` accessing the capability. Must not be opaque.
  url::Origin origin;

  ContentSettingsType type;

  // The time interval in which the capability was accessed, such that
  // `usage_start` <= `usage_end`, and neither is null.
  base::Time usage_start;
  base::Time usage_end;

  // Specifies if the permission usage started with the browsing context having
  // transient user activation.
  bool had_user_activation;

  // Specifies if the permission usage started in the foreground.
  bool was_foreground;

  // Specifies if the requesting frame had focus at the time the permission.
  // usage started.
  bool had_focus;

  bool operator==(const PermissionUsageSession& other) const;
  bool operator!=(const PermissionUsageSession& other) const;

  // Checks if the session satisfies the following constraints:
  // 1) `origin` is not opaque;
  // 2) `usage_start` and `usage_end` are not null;
  // 3) `usage_start` <= `usage_end`.
  bool IsValid() const;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_USAGE_SESSION_H_
