// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_AUDITING_SERVICE_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_AUDITING_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace base {
class FilePath;
class Time;
class TimeDelta;
}  // namespace base

namespace permissions {

class PermissionAuditingDatabase;
struct PermissionUsageSession;

// Keeps a client-side log of when websites use permission-gated capabilities to
// allow the user to audit usage.
//
// For each combination of permission type and origin, the history is
// stored on disk as a series of PermissionUsageSessions. Sessions expire
// at the latest after 3 months, or when browsing data or history is cleared.
class PermissionAuditingService final : public KeyedService {
 public:
  typedef base::OnceCallback<void(std::vector<PermissionUsageSession>)>
      PermissionUsageHistoryCallback;

  typedef base::OnceCallback<void(std::optional<base::Time>)>
      LastPermissionUsageTimeCallback;

  explicit PermissionAuditingService(
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);

  ~PermissionAuditingService() override;

  // Initializes Permission Auditing database in `database_path`.
  void Init(const base::FilePath& database_path);

  // Starts the periodic deletions of outdated sessions.
  void StartPeriodicCullingOfExpiredSessions();

  // Appends a new permission usage `session` of the given permission `type` on
  // a given `origin`. `session` must be valid according to IsValid().
  // Operation will fail if a session with the same primary key, that is,
  // origin, type, and usage start time, already exists.
  void StorePermissionUsage(const PermissionUsageSession& session);

  // Returns the detailed history stored for the permission `type` on a given
  // `origin` from the specified `start_time` inclusive. The `origin` must not
  // be opaque. History is provided via `result_callback`. History isn't ordered
  // in any way.
  void GetPermissionUsageHistory(
      ContentSettingsType type,
      const url::Origin& origin,
      base::Time start_time,
      PermissionUsageHistoryCallback result_callback);

  // Returns when the given permission `type` was last used on a given `origin`.
  // The `origin` must not be opaque. Time is provided via `result_callback`.
  void GetLastPermissionUsageTime(
      ContentSettingsType type,
      const url::Origin& origin,
      LastPermissionUsageTimeCallback result_callback);

  // Updates the usage end time for a specific usage session. The session is
  // identified by the primary key {`type`, `origin`, `start_time`}, and must
  // already exist. `start_time` and `new_end_time` must be not null, and
  // `start_time` must be less than or equal to `new_end_time`.
  void UpdateEndTime(ContentSettingsType type,
                     const url::Origin& origin,
                     base::Time start_time,
                     base::Time new_end_time);

  // Deletes permission usage sessions, which started or ended in the given
  // time range inclusive. A null `start_time` or `end_time` time is treated as
  // -inf and +inf, respectively.
  void DeleteSessionsBetween(base::Time start, base::Time end);

  // Returns sessions maximum lifetime.
  static base::TimeDelta GetUsageSessionMaxAge();

  // Returns the time delta between two consequent expiration iterations.
  static base::TimeDelta GetUsageSessionCullingInterval();

  base::WeakPtr<PermissionAuditingService> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void ExpireOldSessions();

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Lives on the |backend_task_runner_|, and must only be accessed on that
  // sequence. It is safe to assume the database is alive as long as |db_| is
  // non-null.
  std::unique_ptr<PermissionAuditingDatabase> db_;

  base::RepeatingTimer timer_;

  base::WeakPtrFactory<PermissionAuditingService> weak_ptr_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_AUDITING_SERVICE_H_
