// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_PENDING_MIGRATION_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_PENDING_MIGRATION_INFO_H_

#include <optional>

#include "base/values.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace web_app {

namespace proto {
class PendingMigrationInfo;
}  // namespace proto

// Represents a migration that has been identified for this app, where this app
// is the source of the migration.
// This class abstracts the behavior of proto::PendingMigrationInfo to avoid to
// including heavy Protobuf headers in other files, leading to faster
// compilation times.
class PendingMigrationInfo {
 public:
  PendingMigrationInfo(webapps::ManifestId manifest_id,
                       MigrationBehavior behavior);

  PendingMigrationInfo(const PendingMigrationInfo&) = default;
  PendingMigrationInfo& operator=(const PendingMigrationInfo&) = default;
  ~PendingMigrationInfo() = default;

  bool operator==(const PendingMigrationInfo&) const = default;

  // Parses a PendingMigrationInfo from a proto::PendingMigrationInfo message.
  // Returns std::nullopt if the message is invalid if the required fields do
  // not exist or contain invalid data.
  static std::optional<PendingMigrationInfo> ParseAndCreate(
      const proto::PendingMigrationInfo& proto);

  // Serializes this PendingMigrationInfo to a proto::PendingMigrationInfo
  // message.
  proto::PendingMigrationInfo ToProto() const;

  base::Value AsDebugValue() const;

  const webapps::ManifestId& manifest_id() const { return manifest_id_; }
  MigrationBehavior behavior() const { return behavior_; }

 private:
  webapps::ManifestId manifest_id_;
  MigrationBehavior behavior_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_PENDING_MIGRATION_INFO_H_
