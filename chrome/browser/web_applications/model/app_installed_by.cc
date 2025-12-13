// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/app_installed_by.h"

#include <algorithm>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "components/sync/base/time.h"

namespace web_app {

AppInstalledBy::AppInstalledBy(base::Time install_api_call_time,
                               GURL requesting_url)
    : install_api_call_time_(install_api_call_time),
      requesting_url_(std::move(requesting_url)) {
  CHECK(!install_api_call_time.is_null())
      << "The API call time must not be null";
  CHECK(requesting_url_.is_valid()) << "Requesting URL must be valid";
}

AppInstalledBy::AppInstalledBy(const AppInstalledBy&) = default;
AppInstalledBy& AppInstalledBy::operator=(const AppInstalledBy&) = default;
AppInstalledBy::AppInstalledBy(AppInstalledBy&&) = default;
AppInstalledBy& AppInstalledBy::operator=(AppInstalledBy&&) = default;
AppInstalledBy::~AppInstalledBy() = default;

// static
std::optional<AppInstalledBy> AppInstalledBy::Parse(
    const proto::InstalledBy& proto) {
  if (!proto.has_install_api_call_time() || !proto.has_requesting_url()) {
    return std::nullopt;
  }

  // Parse URL from proto.
  GURL requesting_url(proto.requesting_url());
  if (!requesting_url.is_valid()) {
    DLOG(ERROR) << "WebApp proto Installed By url parse error: "
                << requesting_url.possibly_invalid_spec();
    return std::nullopt;
  }

  return AppInstalledBy(syncer::ProtoTimeToTime(proto.install_api_call_time()),
                        std::move(requesting_url));
}

proto::InstalledBy AppInstalledBy::ToProto() const {
  proto::InstalledBy proto;
  proto.set_install_api_call_time(
      syncer::TimeToProtoTime(install_api_call_time_));
  proto.set_requesting_url(requesting_url_.spec());
  return proto;
}

base::DictValue AppInstalledBy::InstalledByToDebugValue() const {
  return base::DictValue()
      .Set("install_api_call_time", base::ToString(install_api_call_time_))
      .Set("requesting_url", requesting_url_.possibly_invalid_spec());
}

}  // namespace web_app
