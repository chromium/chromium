// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "base/version.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace web_app {

inline constexpr base::StringPiece kUpdateManifestAllVersionsKey = "versions";
inline constexpr base::StringPiece kUpdateManifestVersionKey = "version";
inline constexpr base::StringPiece kUpdateManifestSrcKey = "src";
inline constexpr base::StringPiece kUpdateManifestChannelsKey = "channels";

using UpdateChannelId = std::string;
using UpdateChannelIdView = std::string_view;

// An Isolated Web App Update Manifest contains a list of versions and download
// URLs of an Isolated Web App. The format is described in more detail here:
// https://github.com/WICG/isolated-web-apps/blob/main/Updates.md#web-application-update-manifest
class UpdateManifest {
 public:
  // This is the name of the default update channel. If no `channels` field is
  // provided for a version, then it is implicitly set to an array containing
  // `kDefaultChannelId`.
  static constexpr UpdateChannelIdView kDefaultUpdateChannelId = "default";

  enum class JsonFormatError {
    kRootNotADictionary,
    kVersionsNotAnArray,
    kVersionEntryNotADictionary,
  };

  // Attempts to convert the provided JSON data into an instance of
  // `UpdateManifest`.
  //
  // Note that at least one version entry is required; otherwise the Update
  // Manifest is treated as invalid.
  //
  // `update_manifest_url` is used to resolve relative `src` URLs in `versions`.
  static base::expected<UpdateManifest, JsonFormatError> CreateFromJson(
      const base::Value& json,
      const GURL& update_manifest_url);

  UpdateManifest(const UpdateManifest& other);
  UpdateManifest& operator=(const UpdateManifest& other);

  ~UpdateManifest();

  class VersionEntry {
   public:
    static base::expected<VersionEntry, absl::monostate> ParseFromJson(
        const base::Value::Dict& version_entry_dict,
        const GURL& update_manifest_url);

    VersionEntry(GURL src,
                 base::Version version,
                 base::flat_set<UpdateChannelId> channels);

    VersionEntry(const VersionEntry& other);
    VersionEntry& operator=(const VersionEntry& other);

    ~VersionEntry();

    GURL src() const;
    base::Version version() const;

    // Each version contains to a set of update channels, which are defined by
    // the IWA's developer. While the field is optional in the spec, it is
    // always present here and set to its spec-defined default value of
    // `["default"]` if not provided.
    const base::flat_set<UpdateChannelId>& channels() const;

   private:
    friend bool operator==(const VersionEntry& a, const VersionEntry& b);

    static base::expected<base::Version, absl::monostate>
    ParseAndValidateVersion(
        base::optional_ref<const base::Value> version_value);

    static base::expected<GURL, absl::monostate> ParseAndValidateSrc(
        base::optional_ref<const base::Value> src_value,
        const GURL& update_manifest_url);

    // Parses the `channels` field value of a version entry and either returns a
    // set of channels on success or an error on failure. If `channels` is not
    // set (i.e., `channels_value` is `absl::nullopt`), then a set containing
    // `kDefaultUpdateChannelId` is returned.
    static base::expected<base::flat_set<UpdateChannelId>, absl::monostate>
    ParseAndValidateChannels(
        base::optional_ref<const base::Value> channels_value);

    GURL src_;
    base::Version version_;
    base::flat_set<UpdateChannelId> channels_;
  };

  const std::vector<VersionEntry>& versions() const { return version_entries_; }

  // Returns the most up to date version contained in the `UpdateManifest` for a
  // given channel. May return `absl::nullopt` if no applicable version is
  // found.
  std::optional<VersionEntry> GetLatestVersion(UpdateChannelIdView channel);

 private:
  explicit UpdateManifest(std::vector<VersionEntry> version_entries);

  std::vector<VersionEntry> version_entries_;
};

bool operator==(const UpdateManifest::VersionEntry& lhs,
                const UpdateManifest::VersionEntry& rhs);

std::ostream& operator<<(std::ostream& os,
                         const UpdateManifest::VersionEntry& version_entry);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_
