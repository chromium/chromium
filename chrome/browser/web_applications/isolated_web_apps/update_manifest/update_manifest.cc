// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_ref.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

namespace {

base::expected<std::vector<UpdateManifest::VersionEntry>,
               UpdateManifest::JsonFormatError>
ParseVersions(const base::Value::List& version_entries_value,
              const GURL& update_manifest_url) {
  base::flat_map<base::Version, UpdateManifest::VersionEntry> version_entry_map;
  for (const auto& version_entry_value : version_entries_value) {
    const base::Value::Dict* version_entry_dict =
        version_entry_value.GetIfDict();
    if (!version_entry_dict) {
      return base::unexpected(
          UpdateManifest::JsonFormatError::kVersionEntryNotADictionary);
    }

    base::expected<UpdateManifest::VersionEntry, absl::monostate>
        version_entry = UpdateManifest::VersionEntry::ParseFromJson(
            *version_entry_dict, update_manifest_url);
    if (!version_entry.has_value()) {
      // Each version entry must at least contain the version number and URL. If
      // a version entry cannot be parsed, it is ignored for forward
      // compatibility reasons.
      continue;
    }

    // Deliberately overwrite a potential previous entry of the same version.
    // This is for forward-compatibility, see
    // https://github.com/WICG/isolated-web-apps/blob/main/Updates.md#web-application-update-manifest
    // for more information.
    version_entry_map.insert_or_assign(version_entry->version(),
                                       *version_entry);
  }

  return base::ToVector(std::move(version_entry_map), [](auto map_entry) {
    auto [version, version_entry] = std::move(map_entry);
    return version_entry;
  });
}

base::expected<base::flat_map<UpdateChannel, UpdateManifest::ChannelMetadata>,
               UpdateManifest::JsonFormatError>
ParseChannels(const base::Value::Dict& channels) {
  base::flat_map<UpdateChannel, UpdateManifest::ChannelMetadata>
      channels_metadata;
  for (const auto [channel_key, channel_value] : channels) {
    const base::Value::Dict* channel_dict = channel_value.GetIfDict();
    if (!channel_dict) {
      return base::unexpected(
          UpdateManifest::JsonFormatError::kChannelNotADictionary);
    }
    auto channel = UpdateChannel::Create(channel_key);
    if (!channel.has_value()) {
      continue;
    }
    std::optional<std::string> display_name = base::OptionalFromPtr(
        channel_dict->FindString(kUpdateManifestChannelNameKey));
    channels_metadata.emplace(
        *channel, UpdateManifest::ChannelMetadata(*channel, display_name));
  }
  return channels_metadata;
}

}  // namespace

// static
const UpdateChannel& UpdateChannel::default_channel() {
  static const base::NoDestructor<UpdateChannel> kDefaultChannel(
      [] { return *UpdateChannel::Create("default"); }());
  return *kDefaultChannel;
}

// static
base::expected<UpdateChannel, absl::monostate> UpdateChannel::Create(
    std::string input) {
  if (input.empty() || !base::IsStringUTF8(input)) {
    return base::unexpected(absl::monostate());
  }
  return UpdateChannel(std::move(input));
}

void PrintTo(const UpdateChannel& channel, std::ostream* os) {
  *os << channel.ToString();
}

UpdateChannel::UpdateChannel(std::string channel) : name_(std::move(channel)) {}

UpdateChannel::UpdateChannel(const UpdateChannel&) = default;

UpdateChannel::UpdateChannel(UpdateChannel&&) = default;

UpdateChannel& UpdateChannel::operator=(const UpdateChannel&) = default;

UpdateChannel& UpdateChannel::operator=(UpdateChannel&&) = default;

UpdateChannel::~UpdateChannel() = default;

bool UpdateChannel::operator==(const UpdateChannel& other) const = default;
auto UpdateChannel::operator<=>(const UpdateChannel& other) const = default;
bool UpdateChannel::operator<(const UpdateChannel& other) const = default;

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

  ASSIGN_OR_RETURN(std::vector<VersionEntry> version_entries,
                   ParseVersions(*versions, update_manifest_url));

  base::flat_map<UpdateChannel, ChannelMetadata> channels_metadata;
  const base::Value* channels =
      json.GetDict().Find(kUpdateManifestAllChannelsKey);
  if (channels) {
    if (!channels->is_dict()) {
      return base::unexpected(JsonFormatError::kChannelsNotADictionary);
    }

    ASSIGN_OR_RETURN(channels_metadata, ParseChannels(channels->GetDict()));
  }

  return UpdateManifest(std::move(version_entries),
                        std::move(channels_metadata));
}

UpdateManifest::UpdateManifest(
    std::vector<VersionEntry> version_entries,
    base::flat_map<UpdateChannel, ChannelMetadata> channels_metadata)
    : version_entries_(std::move(version_entries)),
      channels_metadata_(std::move(channels_metadata)) {}

UpdateManifest::UpdateManifest(const UpdateManifest& other) = default;

UpdateManifest& UpdateManifest::operator=(const UpdateManifest& other) =
    default;

UpdateManifest::~UpdateManifest() = default;

std::optional<UpdateManifest::VersionEntry> UpdateManifest::GetLatestVersion(
    const UpdateChannel& channel) const {
  std::optional<VersionEntry> latest_version_entry;
  for (const VersionEntry& version_entry : version_entries_) {
    if (!version_entry.channels().contains(channel)) {
      // Ignore version entries that are not part of the provided
      // `channel`.
      continue;
    }
    if (!latest_version_entry.has_value() ||
        latest_version_entry->version() <= version_entry.version()) {
      latest_version_entry = version_entry;
    }
  }
  return latest_version_entry;
}

UpdateManifest::ChannelMetadata UpdateManifest::GetChannelMetadata(
    const UpdateChannel& channel) const {
  const ChannelMetadata* channel_metadata =
      base::FindOrNull(channels_metadata_, channel);
  if (!channel_metadata) {
    return ChannelMetadata(channel,
                           /*display_name=*/std::nullopt);
  }
  return *channel_metadata;
}

// static
base::expected<UpdateManifest::VersionEntry, absl::monostate>
UpdateManifest::VersionEntry::ParseFromJson(
    const base::Value::Dict& version_entry_dict,
    const GURL& update_manifest_url) {
  ASSIGN_OR_RETURN(auto version,
                   ParseAndValidateVersion(
                       version_entry_dict.Find(kUpdateManifestVersionKey)));
  ASSIGN_OR_RETURN(auto src, ParseAndValidateSrc(
                                 version_entry_dict.Find(kUpdateManifestSrcKey),
                                 update_manifest_url));
  ASSIGN_OR_RETURN(auto channels,
                   ParseAndValidateChannels(
                       version_entry_dict.Find(kUpdateManifestChannelsKey)));
  return VersionEntry(std::move(src), std::move(version), std::move(channels));
}

UpdateManifest::ChannelMetadata::ChannelMetadata(
    UpdateChannel channel,
    std::optional<std::string> name)
    : channel_(std::move(channel)), display_name_(std::move(name)) {}

UpdateManifest::ChannelMetadata::ChannelMetadata(const ChannelMetadata& other) =
    default;
UpdateManifest::ChannelMetadata& UpdateManifest::ChannelMetadata::operator=(
    const ChannelMetadata& other) = default;

UpdateManifest::ChannelMetadata::~ChannelMetadata() = default;

bool UpdateManifest::ChannelMetadata::operator==(
    const ChannelMetadata& other) const = default;

void PrintTo(const UpdateManifest::ChannelMetadata& channel_metadata,
             std::ostream* os) {
  *os << base::Value::Dict()
             .Set("channel", base::ToString(channel_metadata.channel_))
             .Set("display_name",
                  channel_metadata.display_name_.has_value()
                      ? base::Value(*channel_metadata.display_name_)
                      : base::Value());
}

UpdateManifest::VersionEntry::VersionEntry(
    GURL src,
    base::Version version,
    base::flat_set<UpdateChannel> channels)
    : src_(std::move(src)),
      version_(std::move(version)),
      channels_(std::move(channels)) {}

UpdateManifest::VersionEntry::VersionEntry(const VersionEntry& other) = default;
UpdateManifest::VersionEntry& UpdateManifest::VersionEntry::operator=(
    const VersionEntry& other) = default;

UpdateManifest::VersionEntry::~VersionEntry() = default;

GURL UpdateManifest::VersionEntry::src() const {
  CHECK(src_.is_valid());
  return src_;
}

base::Version UpdateManifest::VersionEntry::version() const {
  CHECK(version_.IsValid());
  return version_;
}

const base::flat_set<UpdateChannel>& UpdateManifest::VersionEntry::channels()
    const {
  return channels_;
}

// static
base::expected<base::Version, absl::monostate>
UpdateManifest::VersionEntry::ParseAndValidateVersion(
    base::optional_ref<const base::Value> version_value) {
  if (!version_value.has_value() || !version_value->is_string()) {
    return base::unexpected(absl::monostate());
  }

  return ParseIwaVersion(version_value->GetString()).transform_error([](auto) {
    return absl::monostate();
  });
}

// static
base::expected<GURL, absl::monostate>
UpdateManifest::VersionEntry::ParseAndValidateSrc(
    base::optional_ref<const base::Value> src_value,
    const GURL& update_manifest_url) {
  if (!src_value.has_value() || !src_value->is_string()) {
    return base::unexpected(absl::monostate());
  }

  GURL src = update_manifest_url.Resolve(src_value->GetString());
  if (!src.is_valid() || src == update_manifest_url) {
    return base::unexpected(absl::monostate());
  }
  if (!src.SchemeIsHTTPOrHTTPS() ||
      !network::IsUrlPotentiallyTrustworthy(src)) {
    // Only https: and http: URLs are supported as the src URL. Also, they need
    // to be "potentially trustworthy", which includes https:, localhost, and
    // origins configured as trustworthy via enterprise policy. The separate
    // check for the scheme is crucial, as file:// and some other URLs are
    // "potentially trustworthy".
    return base::unexpected(absl::monostate());
  }

  return src;
}

// static
base::expected<base::flat_set<UpdateChannel>, absl::monostate>
UpdateManifest::VersionEntry::ParseAndValidateChannels(
    base::optional_ref<const base::Value> channels_value) {
  if (!channels_value.has_value()) {
    // If the "channels" field is not present in the version entry of the Update
    // Manifest, we treat it as if it was present and contained a single
    // "default" channel.
    return base::flat_set<UpdateChannel>{UpdateChannel::default_channel()};
  }

  if (!channels_value->is_list()) {
    return base::unexpected(absl::monostate());
  }

  std::vector<UpdateChannel> channels;
  for (const auto& channel_value : channels_value->GetList()) {
    const std::string* channel_name_string = channel_value.GetIfString();
    if (!channel_name_string) {
      return base::unexpected(absl::monostate());
    }
    auto channel = UpdateChannel::Create(*channel_name_string);
    if (!channel.has_value()) {
      return base::unexpected(absl::monostate());
    }
    channels.emplace_back(*channel);
  }

  return channels;
}

bool operator==(const UpdateManifest::VersionEntry& lhs,
                const UpdateManifest::VersionEntry& rhs) = default;

std::ostream& operator<<(std::ostream& os,
                         const UpdateManifest::VersionEntry& version_entry) {
  base::Value::List channels;
  for (const auto& channel : version_entry.channels()) {
    channels.Append(channel.ToString());
  }
  return os << base::Value::Dict()
                   .Set(kUpdateManifestSrcKey, version_entry.src().spec())
                   .Set(kUpdateManifestVersionKey,
                        version_entry.version().GetString())
                   .Set(kUpdateManifestChannelsKey, std::move(channels));
}

}  // namespace web_app
