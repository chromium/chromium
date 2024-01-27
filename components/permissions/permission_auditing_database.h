// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_AUDITING_DATABASE_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_AUDITING_DATABASE_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_usage_session.h"
#include "sql/database.h"
#include "url/origin.h"

namespace base {
class FilePath;
}  // namespace base

namespace permissions {

// Stores permission usage sessions for specific url origin and
// ContentSettingType in an SQLite database. Additionally, handles the queries
// about the last permission usage time for a specific origin.
// Threading constraints:
// 1) This class is not thread-safe, so each instance must be used on the same
// sequence;
// 2) Instances must be used on a sequence that can execute blocking tasks.
class PermissionAuditingDatabase {
 public:
  PermissionAuditingDatabase();
  ~PermissionAuditingDatabase();

  PermissionAuditingDatabase(const PermissionAuditingDatabase&) = delete;
  PermissionAuditingDatabase& operator=(const PermissionAuditingDatabase&) =
      delete;

  PermissionAuditingDatabase(PermissionAuditingDatabase&&) = delete;
  PermissionAuditingDatabase& operator=(const PermissionAuditingDatabase&&) =
      delete;

  // Opens an existing database at `path` or creates a new one if none exists,
  // and returns true on success.
  bool Init(const base::FilePath& path);

  // Appends a new permission usage `session` of the given permission `type` on
  // a given `origin`. The `session` must be valid according to IsValid().
  // Operation will fail if a session with the same primary key, that
  // is, origin, type, and usage start time, already exists in the database.
  // Returns if the operation was successful.
  bool StorePermissionUsage(const PermissionUsageSession& session);

  // Returns the detailed history stored for the permission `type` on a given
  // `origin` from the specified `start_time`. The `origin` must not be opaque.
  std::vector<PermissionUsageSession> GetPermissionUsageHistory(
      ContentSettingsType type,
      const url::Origin& origin,
      base::Time start_time);

  // Returns when the given permission `type` was last used on a given `origin`.
  // Returns nullopt if no permission usages match the given constraints. The
  // `origin` must not be opaque.
  std::optional<base::Time> GetLastPermissionUsageTime(
      ContentSettingsType type,
      const url::Origin& origin);

  // Updates the usage end time for a specific usage session. The session is
  // identified by the primary key {`type`, `origin`, `start_time`}, and must
  // already exist. `start_time` must be less than or equal to `new_end_time`.
  // Operation will fail if `start_time` or `new_end_time` is null. Returns if
  // the operation was successful.
  bool UpdateEndTime(ContentSettingsType type,
                     const url::Origin& origin,
                     base::Time start_time,
                     base::Time new_end_time);

  // Deletes permission usage sessions, which started or ended in the given
  // time range. A null `start_time` or `end_time` time is treated as -inf and
  // +inf, respectively. Returns if the operation was successful.
  bool DeleteSessionsBetween(base::Time start_time, base::Time end_time);

 private:
  bool CreateSchema();

  // The SQL connection to database.
  sql::Database db_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_AUDITING_DATABASE_H_
