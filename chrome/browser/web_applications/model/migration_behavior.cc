// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/migration_behavior.h"

#include "base/notreached.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "third_party/blink/public/mojom/manifest/manifest_migration_behavior.mojom.h"

namespace web_app {

proto::WebAppMigrationBehavior ToProtoMigrationBehavior(
    MigrationBehavior behavior) {
  switch (behavior) {
    case MigrationBehavior::kForce:
      return proto::WebAppMigrationBehavior::WEB_APP_MIGRATION_BEHAVIOR_FORCE;
    case MigrationBehavior::kSuggest:
      return proto::WebAppMigrationBehavior::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST;
  }
  NOTREACHED();
}

bool IsValidProtoMigrationBehavior(
    proto::WebAppMigrationBehavior proto_behavior) {
  switch (proto_behavior) {
    case proto::WebAppMigrationBehavior::WEB_APP_MIGRATION_BEHAVIOR_FORCE:
    case proto::WebAppMigrationBehavior::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST:
      return true;
    default:
      return false;
  }
}

MigrationBehavior FromProtoMigrationBehavior(
    proto::WebAppMigrationBehavior proto_behavior) {
  switch (proto_behavior) {
    case proto::WebAppMigrationBehavior::WEB_APP_MIGRATION_BEHAVIOR_FORCE:
      return MigrationBehavior::kForce;
    case proto::WebAppMigrationBehavior::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST:
      return MigrationBehavior::kSuggest;
    case proto::WebAppMigrationBehavior::WEB_APP_MIGRATION_BEHAVIOR_UNSPECIFIED:
      NOTREACHED();
  }
}

}  // namespace web_app
