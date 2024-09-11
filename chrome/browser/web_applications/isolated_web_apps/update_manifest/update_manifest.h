// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "base/version.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace web_app {

inline constexpr std::string_view kUpdateManifestAllVersionsKey = "versions";
inline constexpr std::string_view kUpdateManifestAllChannelsKey = "channels";
inline constexpr std::string_view kUpdateManifestChannelNameKey = "name";
inline constexpr std::string_view kUpdateManifestVersionKey = "version";
inline constexpr std::string_view kUpdateManifestSrcKey = "src";
inline constexpr std::string_view kUpdateManifestChannelsKey = "channels";

class UpdateChannelId {
 public:
  // Returns an instance of the "default" update channel ID.
  static const UpdateChannelId& default_id();

  static base::expected<UpdateChannelId, absl::monostate> Create(
      std::string input);

  UpdateChannelId(const UpdateChannelId&);
  UpdateChannelId(UpdateChannelId&&);
  UpdateChannelId& operator=(const UpdateChannelId&);
  UpdateChannelId& operator=(UpdateChannelId&&);

  ~UpdateChannelId();

  bool operator==(const UpdateChannelId& other) const;
  auto operator<=>(const UpdateChannelId& other) const;
  bool operator<(const UpdateChannelId& other) const;

  const std::string& ToString() const { return id_; }

  // For gtest
  friend void PrintTo(const UpdateChannelId& id, std::ostream* os);

 private:
  explicit UpdateChannelId(std::string id);

  std::string id_;
};

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
    static base::expected<ChannelMetadata, absl::monostate> ParseFromJson(
        const base::Value::Dict& channel_metadata_dict);

    ChannelMetadata(UpdateChannelId id, std::optional<std::string> name);

    ChannelMetadata(const ChannelMetadata& other);
    ChannelMetadata& operator=(const ChannelMetadata& other);

    ~ChannelMetadata();

    bool operator==(const ChannelMetadata& other) const;

    // For gtest
    friend void PrintTo(const ChannelMetadata& channel_metadata,
                        std::ostream* os);

    // Returns the channel's name if available, or the channel ID otherwise.
    std::string GetDisplayName() const {
      return name_.value_or(id_.ToString());
    }

    const UpdateChannelId& id() const { return id_; }
    const std::optional<std::string>& name() const { return name_; }

   private:
    UpdateChannelId id_;
    std::optional<std::string> name_;
  };

  class VersionEntry {
   public:
    static base::expected<VersionEntry, absl::monostate> ParseFromJson(
        const base::Value::Dict& version_entry_dict,
        const GURL& update_manifest_url);

    VersionEntry(GURL src,
                 base::Version version,
                 base::flat_set<UpdateChannelId> channel_ids);

    VersionEntry(const VersionEntry& other);
    VersionEntry& operator=(const VersionEntry& other);

    ~VersionEntry();

    GURL src() const;
    base::Version version() const;

    // Each version contains to a set of update channels, which are defined by
    // the IWA's developer. While the field is optional in the spec, it is
    // always present here and set to its spec-defined default value of
    // `["default"]` if not provided.
    const base::flat_set<UpdateChannelId>& channel_ids() const;

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
    // set (i.e., `channels_value` is `std::nullopt`), then a set containing
    // just the "default" channel is returned.
    static base::expected<base::flat_set<UpdateChannelId>, absl::monostate>
    ParseAndValidateChannels(
        base::optional_ref<const base::Value> channels_value);

    GURL src_;
    base::Version version_;
    base::flat_set<UpdateChannelId> channel_ids_;
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
  // given `channel_id`. May return `std::nullopt` if no applicable version is
  // found.
  std::optional<VersionEntry> GetLatestVersion(
      const UpdateChannelId& channel_id) const;

  // Returns channel metadata for a provided update channel ID. If no metadata
  // for the provided channel ID is present in the Update Manifest, then this
  // will still return an empty `ChannelMetadata` instance for that channel ID.
  ChannelMetadata GetChannelMetadata(const UpdateChannelId& channel_id) const;

 private:
  explicit UpdateManifest(
      std::vector<VersionEntry> version_entries,
      base::flat_map<UpdateChannelId, ChannelMetadata> channels_metadata);

  std::vector<VersionEntry> version_entries_;
  base::flat_map<UpdateChannelId, ChannelMetadata> channels_metadata_;
};

bool operator==(const UpdateManifest::VersionEntry& lhs,
                const UpdateManifest::VersionEntry& rhs);

std::ostream& operator<<(std::ostream& os,
                         const UpdateManifest::VersionEntry& version_entry);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_
