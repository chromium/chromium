// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "url/gurl.h"

namespace web_app {

inline constexpr std::string_view kUpdateManifestAllVersionsKey = "versions";
inline constexpr std::string_view kUpdateManifestAllChannelsKey = "channels";
inline constexpr std::string_view kUpdateManifestChannelNameKey = "name";
inline constexpr std::string_view kUpdateManifestVersionKey = "version";
inline constexpr std::string_view kUpdateManifestSrcKey = "src";
inline constexpr std::string_view kUpdateManifestChannelsKey = "channels";

// An Isolated Web App Update Manifest contains a list of versions and download
// URLs of an Isolated Web App. The format is described in more detail here:
// https://github.com/WICG/isolated-web-apps/blob/main/Updates.md#web-application-update-manifest
class UpdateManifest {
 public:
  enum class JsonFormatError {
    kRootNotADictionary,
    kChannelsNotADictionary,
    kChannelNotADictionary,
    kVersionsNotAnArray,
    kVersionEntryNotADictionary,
  };

  class ChannelMetadata {
   public:
    static base::expected<ChannelMetadata, std::monostate> ParseFromJson(
        const base::Value::Dict& channel_metadata_dict);

    ChannelMetadata(UpdateChannel update_channel,
                    std::optional<std::string> display_name);

    ChannelMetadata(const ChannelMetadata& other);
    ChannelMetadata& operator=(const ChannelMetadata& other);

    ~ChannelMetadata();

    bool operator==(const ChannelMetadata& other) const;

    // For gtest
    friend void PrintTo(const ChannelMetadata& channel_metadata,
                        std::ostream* os);

    // Returns the channel's display name if available, or the channel name
    // otherwise.
    std::string GetDisplayName() const {
      return display_name_.value_or(channel_.ToString());
    }

    const UpdateChannel& channel() const { return channel_; }
    const std::optional<std::string>& display_name() const {
      return display_name_;
    }

   private:
    UpdateChannel channel_;
    std::optional<std::string> display_name_;
  };

  class VersionEntry {
   public:
    static base::expected<VersionEntry, std::monostate> ParseFromJson(
        const base::Value::Dict& version_entry_dict,
        const GURL& update_manifest_url);

    VersionEntry(GURL src,
                 IwaVersion version,
                 base::flat_set<UpdateChannel> channels);

    VersionEntry(const VersionEntry& other);
    VersionEntry& operator=(const VersionEntry& other);

    ~VersionEntry();

    GURL src() const;
    IwaVersion version() const;

    // Each version contains to a set of update channels, which are defined by
    // the IWA's developer. While the field is optional in the spec, it is
    // always present here and set to its spec-defined default value of
    // `["default"]` if not provided.
    const base::flat_set<UpdateChannel>& channels() const;

   private:
    friend bool operator==(const VersionEntry& a, const VersionEntry& b);

    GURL src_;
    IwaVersion version_;
    base::flat_set<UpdateChannel> channels_;
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

  const std::vector<VersionEntry>& versions() const { return version_entries_; }

  // Returns the most up to date version contained in the `UpdateManifest` for a
  // given `channel`. May return `std::nullopt` if no applicable version is
  // found.
  std::optional<VersionEntry> GetLatestVersion(
      const UpdateChannel& channel) const;

  // Returns version entry for given version and channel. If there is no entry
  // matching the criteria, then it returns `std::nullopt'.
  std::optional<VersionEntry> GetVersion(const IwaVersion& version,
                                         const UpdateChannel& channel) const;

  // Returns channel metadata for a provided update channel ID. If no metadata
  // for the provided channel ID is present in the Update Manifest, then this
  // will still return an empty `ChannelMetadata` instance for that channel ID.
  ChannelMetadata GetChannelMetadata(const UpdateChannel& channel) const;

 private:
  explicit UpdateManifest(
      std::vector<VersionEntry> version_entries,
      base::flat_map<UpdateChannel, ChannelMetadata> channels_metadata);

  std::vector<VersionEntry> version_entries_;
  base::flat_map<UpdateChannel, ChannelMetadata> channels_metadata_;
};

bool operator==(const UpdateManifest::VersionEntry& lhs,
                const UpdateManifest::VersionEntry& rhs);

std::ostream& operator<<(std::ostream& os,
                         const UpdateManifest::VersionEntry& version_entry);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_
