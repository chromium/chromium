// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/bundle_versions_storage.h"

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/containers/to_value_list.h"
#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"

namespace web_app::test {

namespace {
constexpr std::string_view kUpdateManifestFileName = "update_manifest.json";

std::string GetRelativeUpdateManifestUrl(
    const web_package::SignedWebBundleId& web_bundle_id) {
  return base::StringPrintf("%s/%s", web_bundle_id.id(),
                            kUpdateManifestFileName);
}

std::string GetRelativeWebBundleUrl(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaVersion& version) {
  return base::StringPrintf("%s/%s.swbn", web_bundle_id.id(),
                            version.GetString());
}

}  // namespace

struct BundleVersionsStorage::BundleInfo {
  BundleInfo(std::unique_ptr<BundledIsolatedWebApp> bundled_app,
             std::optional<std::vector<UpdateChannel>> update_channels)
      : bundle(std::move(bundled_app)),
        update_channels(std::move(update_channels)) {}

  std::unique_ptr<BundledIsolatedWebApp> bundle;
  std::optional<std::vector<UpdateChannel>> update_channels;
};

BundleVersionsStorage::BundleVersionsStorage() = default;
BundleVersionsStorage::~BundleVersionsStorage() = default;

void BundleVersionsStorage::SetBaseUrl(const GURL& base_url) {
  CHECK(!base_url_);
  base_url_ = base_url;
}

// static
GURL BundleVersionsStorage::GetUpdateManifestUrl(
    const GURL& base_url,
    const web_package::SignedWebBundleId& web_bundle_id) {
  return base_url.Resolve(GetRelativeUpdateManifestUrl(web_bundle_id));
}

GURL BundleVersionsStorage::GetUpdateManifestUrl(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  return GetUpdateManifestUrl(*base_url_, web_bundle_id);
}

base::Value::Dict BundleVersionsStorage::GetUpdateManifest(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  const auto& bundle_versions =
      CHECK_DEREF(base::FindOrNull(bundle_versions_per_id_, web_bundle_id));
  return base::Value::Dict().Set(
      "versions",
      base::ToValueList(bundle_versions, [&](const auto& bundle_meta) {
        const auto& [version, bundle_info] = bundle_meta;

        auto dict = base::Value::Dict()
                        .Set("version", version.GetString())
                        .Set("src", base_url_
                                        ->Resolve(GetRelativeWebBundleUrl(
                                            web_bundle_id, version))
                                        .spec());

        if (bundle_info->update_channels) {
          dict.Set("channels",
                   base::ToValueList(bundle_info->update_channels.value(),
                                     &UpdateChannel::ToString));
        }
        return dict;
      }));
}

GURL BundleVersionsStorage::AddBundle(
    std::unique_ptr<BundledIsolatedWebApp> bundle,
    std::optional<std::vector<UpdateChannel>> update_channels) {
  CHECK(base_url_)
      << "SetBaseUrl() must be invoked prior to the first call to AddBundle(). "
         "If you're using IsolatedWebAppTestUpdateServer, make sure that "
         "AddBundle() is called from SetUpOnMainThread() and not from the "
         "constructor.";

  auto web_bundle_id = bundle->web_bundle_id();
  auto version = IwaVersion::Create(bundle->version().GetString()).value();
  bundle_versions_per_id_[web_bundle_id][version] =
      std::make_unique<BundleInfo>(std::move(bundle),
                                   std::move(update_channels));
  return base_url_->Resolve(GetRelativeWebBundleUrl(web_bundle_id, version));
}

void BundleVersionsStorage::RemoveBundle(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaVersion& version) {
  CHECK(base::Contains(bundle_versions_per_id_, web_bundle_id));
  auto& bundle_versions = bundle_versions_per_id_[web_bundle_id];
  CHECK(base::Contains(bundle_versions, version));
  bundle_versions.erase(version);
  if (bundle_versions.empty()) {
    bundle_versions_per_id_.erase(web_bundle_id);
  }
}

std::optional<BundleVersionsStorage::BundleOrUpdateManifest>
BundleVersionsStorage::GetResource(const std::string& route) {
  // Parses /<web_bundle_id>/<file_name> into { <web_bundle_id>, <file_name> }.
  auto pieces = base::SplitString(route, "/", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() != 2) {
    return std::nullopt;
  }

  ASSIGN_OR_RETURN(auto web_bundle_id,
                   web_package::SignedWebBundleId::Create(pieces[0]),
                   [](auto) -> std::optional<BundleOrUpdateManifest> {
                     return std::nullopt;
                   });
  const auto* bundle_versions =
      base::FindOrNull(bundle_versions_per_id_, web_bundle_id);
  if (!bundle_versions) {
    return std::nullopt;
  }

  const auto& path = pieces[1];
  if (path == kUpdateManifestFileName) {
    return GetUpdateManifest(web_bundle_id);
  } else if (path.ends_with(".swbn")) {
    auto iwa_version = IwaVersion::Create(path.substr(0, path.size() - 5));
    if (iwa_version.has_value()) {
      if (auto* bundle_info =
              base::FindPtrOrNull(*bundle_versions, *std::move(iwa_version))) {
        return bundle_info->bundle.get();
      }
    }
  }

  return std::nullopt;
}

}  // namespace web_app::test
