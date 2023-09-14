// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"

#include <array>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace web_app {

// static
base::expected<UpdateManifest, UpdateManifest::JsonFormatError>
UpdateManifest::CreateFromJson(const base::Value& json,
                               const GURL& update_manifest_url) {
  if (!json.is_dict()) {
    return base::unexpected(JsonFormatError::kRootNotADictionary);
  }

  const base::Value::List* versions =
      json.GetDict().FindList(kUpdateManifestAllVersionsKey);
  if (!versions) {
    return base::unexpected(JsonFormatError::kVersionsNotAnArray);
  }

  base::flat_map<base::Version, VersionEntry> version_entry_map;
  for (const auto& version_entry : *versions) {
    const base::Value::Dict* version_entry_dict = version_entry.GetIfDict();
    if (!version_entry_dict) {
      return base::unexpected(JsonFormatError::kVersionEntryNotADictionary);
    }

    const std::string* version_string =
        version_entry_dict->FindString(kUpdateManifestVersionKey);
    const std::string* src_string =
        version_entry_dict->FindString(kUpdateManifestSrcKey);
    if (!version_string || !src_string) {
      // Each version entry must at least contain the version number and URL.
      continue;
    }

    base::expected<std::array<uint32_t, 3>, IwaVersionParseError>
        version_components = ParseIwaVersionIntoComponents(*version_string);
    if (!version_components.has_value()) {
      continue;
    }
    base::Version version(std::vector<uint32_t>(version_components->begin(),
                                                version_components->end()));
    CHECK(version.IsValid());

    GURL src = update_manifest_url.Resolve(*src_string);
    if (!src.is_valid() || src == update_manifest_url) {
      continue;
    }
    if (!src.SchemeIsHTTPOrHTTPS() ||
        !network::IsUrlPotentiallyTrustworthy(src)) {
      // Only https: and http: URLs are supported as the src URL. Also, they
      // need to be "potentially trustworthy", which includes https:, localhost,
      // and origins configured as trustworthy via enterprise policy. The
      // separate check for the scheme is crucial, as file:// and some other
      // URLs are "potentially trustworthy".
      continue;
    }

    // Deliberately overwrite a potential previous entry of the same version.
    // This is for forward-compatibility, see
    // https://github.com/WICG/isolated-web-apps/blob/main/Updates.md#web-application-update-manifest
    // for more information.
    version_entry_map.insert_or_assign(version, VersionEntry(src, version));
  }

  std::vector<VersionEntry> version_entries;
  for (auto& [version, version_entry] : version_entry_map) {
    version_entries.emplace_back(std::move(version_entry));
  }

  if (version_entries.empty()) {
    // The update manifest must contain at least one version entry, otherwise it
    // is treated as invalid.
    return base::unexpected(JsonFormatError::kNoApplicableVersion);
  }

  return UpdateManifest(version_entries);
}

UpdateManifest::UpdateManifest(std::vector<VersionEntry> version_entries)
    : version_entries_(std::move(version_entries)) {
  CHECK(!version_entries_.empty());
}

UpdateManifest::UpdateManifest(const UpdateManifest& other) = default;

UpdateManifest& UpdateManifest::operator=(const UpdateManifest& other) =
    default;

UpdateManifest::~UpdateManifest() = default;

UpdateManifest::VersionEntry::VersionEntry(GURL src, base::Version version)
    : src_(src), version_(version) {}

GURL UpdateManifest::VersionEntry::src() const {
  CHECK(src_.is_valid());
  return src_;
}

base::Version UpdateManifest::VersionEntry::version() const {
  CHECK(version_.IsValid());
  return version_;
}

UpdateManifest::VersionEntry GetLatestVersionEntry(
    const UpdateManifest& update_manifest) {
  return base::ranges::max(
      update_manifest.versions(), base::ranges::less(),
      [](const UpdateManifest::VersionEntry& version_entry) {
        return version_entry.version();
      });
}

bool operator==(const UpdateManifest::VersionEntry& lhs,
                const UpdateManifest::VersionEntry& rhs) = default;

}  // namespace web_app
