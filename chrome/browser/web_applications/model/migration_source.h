// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MIGRATION_SOURCE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MIGRATION_SOURCE_H_

#include <optional>

#include "base/values.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace web_app {

namespace proto {
class WebAppMigrationSource;
}  // namespace proto

// Represents a migration source specified in the 'migration_from' field of a
// web app manifest.
// This class abstracts the behavior of proto::WebAppMigrationSource to avoid
// including heavy Protobuf headers in other files, leading to faster
// compilation times.
class MigrationSource {
 public:
  MigrationSource(webapps::ManifestId manifest_id,
                  MigrationBehavior behavior,
                  std::optional<GURL> install_url = std::nullopt);
  MigrationSource(const MigrationSource&);
  MigrationSource& operator=(const MigrationSource&);
  ~MigrationSource();

  bool operator==(const MigrationSource&) const = default;

  // Provides a single point of validation when loading migration data from the
  // database. This ensures that the in-memory model only ever contains
  // well-formed migration sources.
  static std::optional<MigrationSource> ParseAndCreate(
      const proto::WebAppMigrationSource& proto);

  // Serializes this MigrationSource to a profo::WebAppMigrationSource.
  proto::WebAppMigrationSource ToProto() const;

  base::Value AsDebugValue() const;

  const webapps::ManifestId& manifest_id() const { return manifest_id_; }
  MigrationBehavior behavior() const { return behavior_; }
  const std::optional<GURL>& install_url() const { return install_url_; }

 private:
  webapps::ManifestId manifest_id_;
  MigrationBehavior behavior_;
  std::optional<GURL> install_url_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_MIGRATION_SOURCE_H_
