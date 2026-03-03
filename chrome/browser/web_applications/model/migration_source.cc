// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/migration_source.h"

#include <utility>

#include "base/check.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "url/origin.h"

namespace web_app {

MigrationSource::MigrationSource(webapps::ManifestId manifest_id,
                                 MigrationBehavior behavior,
                                 std::optional<GURL> install_url)
    : manifest_id_(std::move(manifest_id)),
      behavior_(behavior),
      install_url_(std::move(install_url)) {
  CHECK(manifest_id_.is_valid());
  CHECK(!url::Origin::Create(manifest_id_).opaque());
  if (install_url_.has_value()) {
    CHECK(install_url_->is_valid());
    CHECK(url::IsSameOriginWith(manifest_id_, *install_url_));
  }
}

MigrationSource::MigrationSource(const MigrationSource&) = default;
MigrationSource& MigrationSource::operator=(const MigrationSource&) = default;
MigrationSource::~MigrationSource() = default;

std::optional<MigrationSource> MigrationSource::ParseAndCreate(
    const proto::WebAppMigrationSource& proto) {
  // Exit early if either field is missing, as both fields are required for a
  // valid `MigrationSource`.
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

  std::optional<GURL> install_url;
  if (proto.has_install_url()) {
    install_url = GURL(proto.install_url());
    if (!install_url->is_valid() ||
        !url::IsSameOriginWith(manifest_id, *install_url)) {
      return std::nullopt;
    }
  }

  return MigrationSource(std::move(manifest_id),
                         FromProtoMigrationBehavior(proto.behavior()),
                         std::move(install_url));
}

proto::WebAppMigrationSource MigrationSource::ToProto() const {
  proto::WebAppMigrationSource proto;
  proto.set_manifest_id(manifest_id_.spec());
  proto.set_behavior(ToProtoMigrationBehavior(behavior_));
  if (install_url_.has_value()) {
    proto.set_install_url(install_url_->spec());
  }
  return proto;
}

base::Value MigrationSource::AsDebugValue() const {
  base::DictValue root;
  root.Set("manifest_id", manifest_id_.possibly_invalid_spec());
  root.Set("behavior", base::ToString(behavior_));
  if (install_url_.has_value()) {
    root.Set("install_url", install_url_->possibly_invalid_spec());
  }
  return base::Value(std::move(root));
}

}  // namespace web_app
