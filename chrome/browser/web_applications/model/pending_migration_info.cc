// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/pending_migration_info.h"

#include <utility>

#include "base/check.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app.to_value.h"
#include "url/origin.h"

namespace web_app {

PendingMigrationInfo::PendingMigrationInfo(webapps::ManifestId manifest_id,
                                           MigrationBehavior behavior)
    : manifest_id_(manifest_id), behavior_(behavior) {
  CHECK(manifest_id_.is_valid())
      << "Manifest id for a pending migration info must be valid";
}

std::optional<PendingMigrationInfo> PendingMigrationInfo::ParseAndCreate(
    // Exit early if either field is missing, as both fields are required for a
    // valid `PendingMigrationInfo`.
    const proto::PendingMigrationInfo& proto) {
  if (!proto.has_manifest_id() || !proto.has_behavior()) {
    return std::nullopt;
  }

  if (!IsValidProtoMigrationBehavior(proto.behavior())) {
    return std::nullopt;
  }

  // The `manifest_id` for the destination app should be valid, otherwise this
  // is an incorrect state for the app to exist as.
  webapps::ManifestId manifest_id(proto.manifest_id());
  if (!manifest_id.is_valid() || url::Origin::Create(manifest_id).opaque()) {
    return std::nullopt;
  }

  return PendingMigrationInfo(manifest_id,
                              FromProtoMigrationBehavior(proto.behavior()));
}

proto::PendingMigrationInfo PendingMigrationInfo::ToProto() const {
  proto::PendingMigrationInfo proto;
  proto.set_manifest_id(manifest_id_.spec());
  proto.set_behavior(ToProtoMigrationBehavior(behavior_));
  return proto;
}

base::Value PendingMigrationInfo::AsDebugValue() const {
  base::DictValue root;
  root.Set("manifest_id", manifest_id_.possibly_invalid_spec());
  root.Set("behavior", base::ToString(behavior_));
  return base::Value(std::move(root));
}

}  // namespace web_app
