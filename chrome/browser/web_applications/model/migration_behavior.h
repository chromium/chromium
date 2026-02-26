// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MIGRATION_BEHAVIOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MIGRATION_BEHAVIOR_H_

#include "third_party/blink/public/mojom/manifest/manifest_migration_behavior.mojom-forward.h"

namespace web_app {

namespace proto {
enum WebAppMigrationBehavior : int;
}  // namespace proto

using MigrationBehavior = blink::mojom::ManifestMigrationBehavior;

proto::WebAppMigrationBehavior ToProtoMigrationBehavior(
    MigrationBehavior behavior);

// Prevent the browser from crashing when `proto_behavior` is UNSPECIFIED or
// invalid. Callsites must ensure the `proto_behavior` is valid before calling
// FromProtoMigrationBehavior().
bool IsValidProtoMigrationBehavior(
    proto::WebAppMigrationBehavior proto_behavior);

// `proto_behavior` must be a valid enum value before
// calling this function. Passing an invalid or UNSPECIFIED value will result in
// a crash.
MigrationBehavior FromProtoMigrationBehavior(
    proto::WebAppMigrationBehavior proto_behavior);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MIGRATION_BEHAVIOR_H_
