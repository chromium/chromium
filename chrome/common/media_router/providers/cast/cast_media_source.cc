// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/providers/cast/cast_media_source.h"

#include <algorithm>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/media_router/media_source.h"
#include "components/cast_channel/enum_table.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_util.h"

using cast_channel::BroadcastRequest;
using cast_channel::CastDeviceCapability;

namespace cast_util {

using media_router::AutoJoinPolicy;
using media_router::DefaultActionPolicy;

template <>
const EnumTable<AutoJoinPolicy> EnumTable<AutoJoinPolicy>::instance(
    {
        {AutoJoinPolicy::kPageScoped, "page_scoped"},
        {AutoJoinPolicy::kTabAndOriginScoped, "tab_and_origin_scoped"},
        {AutoJoinPolicy::kOriginScoped, "origin_scoped"},
    },
    AutoJoinPolicy::kMaxValue);

template <>
const EnumTable<DefaultActionPolicy> EnumTable<DefaultActionPolicy>::instance(
    {
        {DefaultActionPolicy::kCreateSession, "create_session"},
        {DefaultActionPolicy::kCastThisTab, "cast_this_tab"},
    },
    DefaultActionPolicy::kMaxValue);

template <>
const EnumTable<CastDeviceCapability> EnumTable<CastDeviceCapability>::instance(
    {
        {CastDeviceCapability::MULTIZONE_GROUP, "multizone_group"},
        {CastDeviceCapability::DEV_MODE, "dev_mode"},
        {CastDeviceCapability::AUDIO_IN, "audio_in"},
        {CastDeviceCapability::AUDIO_OUT, "audio_out"},
        {CastDeviceCapability::VIDEO_IN, "video_in"},
        {CastDeviceCapability::VIDEO_OUT, "video_out"},
        // NONE deliberately omitted
    },
    NonConsecutiveEnumTable);

}  // namespace cast_util

namespace media_router {

namespace {

// A nonmember version of base::Optional::value_or that works on pointers as
// well as instance of base::Optional.
template <typename T>
inline auto value_or(const T& optional,
                     const std::decay_t<decltype(*optional)>& default_value)
    -> std::decay_t<decltype(*optional)> {
  return optional ? *optional : default_value;
}

// FindValue() looks up the value associated with a key |key| in a map-like
// object |map| and returns a pointer to the value if |key| is found, or nullptr
// otherwise.
//
// The type of |map| can be anything that supports a find() method like
// std::map::find, or any iterable object whose values are key/value pairs.
//
// See also FindValueOr().

// Overload for types with a find() method.
template <typename Map, typename = typename Map::key_type>
inline const typename Map::mapped_type* FindValue(
    const Map& map,
    const typename Map::key_type& key) {
  auto it = map.find(key);
  if (it == map.end())
    return nullptr;
  return &it->second;
}

// Overload for types without a find() method.
template <typename Map, typename Key>
auto FindValue(const Map& map, const Key& key) -> const
    decltype(begin(map)->second)* {
  for (const auto& item : map) {
    if (item.first == key)
      return &item.second;
  }
  return nullptr;
}

// Looks up the value associated with a key |key| in a map-like object |map| and
// returns a reference to the value if |key| is found, or |default_value|
// otherwise.
//
// The type of |map| can be anything that supports a find() method like
// std::map::find, or any iterable object whose values are key/value pairs.
template <typename Map, typename Key, typename T>
inline auto FindValueOr(const Map& map, const Key& key, const T& default_value)
    -> std::decay_t<decltype(*FindValue(map, key))> {
  return value_or(FindValue(map, key), default_value);
}

// Creates a map from the query parameters of |url|.  If |url| contains multiple
// values for the same parameter, the last value is used.
base::flat_map<std::string, std::string> MakeQueryMap(const GURL& url) {
  base::flat_map<std::string, std::string> result;
  for (net::QueryIterator query_it(url); !query_it.IsAtEnd();
       query_it.Advance()) {
    result[query_it.GetKey()] = query_it.GetUnescapedValue();
  }
  return result;
}

// TODO(jrw): Move to common utils?
//
// TODO(jrw): Should this use net::UnescapeURLComponent instead of
// url::DecodeURLEscapeSequences?
std::string DecodeURLComponent(const std::string& encoded) {
  url::RawCanonOutputT<base::char16> unescaped;
  std::string output;
  url::DecodeURLEscapeSequences(encoded.data(), encoded.size(),
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped);
  if (base::UTF16ToUTF8(unescaped.data(), unescaped.length(), &output))
    return output;

  return std::string();
}

// Converts a string containing a comma-separated list of capabilities into a
// bitwise OR of CastDeviceCapability values.
BitwiseOr<CastDeviceCapability> CastDeviceCapabilitiesFromString(
    const base::StringPiece& s) {
  BitwiseOr<CastDeviceCapability> result{};
  for (const auto& capability_str : base::SplitStringPiece(
           s, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    const auto capability =
        cast_util::StringToEnum<CastDeviceCapability>(capability_str);
    if (capability) {
      result.Add(*capability);
    } else {
      DLOG(ERROR) << "Unkown capability name: " << capability_str;
    }
  }
  return result;
}

std::unique_ptr<CastMediaSource> CastMediaSourceForTabMirroring(
    const MediaSource::Id& source_id) {
  return std::make_unique<CastMediaSource>(
      source_id,
      std::vector<CastAppInfo>({CastAppInfo(kCastStreamingAppId),
                                CastAppInfo(kCastStreamingAudioAppId)}));
}

std::unique_ptr<CastMediaSource> CastMediaSourceForDesktopMirroring(
    const MediaSource::Id& source_id) {
  // TODO(https://crbug.com/849335): Add back audio-only devices for desktop
  // mirroring when proper support is implemented.
  return std::make_unique<CastMediaSource>(
      source_id, std::vector<CastAppInfo>({CastAppInfo(kCastStreamingAppId)}));
}

// The logic shared by ParseCastUrl() and ParseLegacyCastUrl().
std::unique_ptr<CastMediaSource> CreateFromURLParams(
    const MediaSource::Id& source_id,
    const std::vector<CastAppInfo>& app_infos,
    const std::string& auto_join_policy_str,
    const std::string& default_action_policy_str,
    const std::string& client_id,
    const std::string& broadcast_namespace,
    const std::string& encoded_broadcast_message,
    const std::string& launch_timeout_str) {
  if (app_infos.empty())
    return nullptr;

  auto cast_source = std::make_unique<CastMediaSource>(
      source_id, app_infos,
      cast_util::StringToEnum<AutoJoinPolicy>(auto_join_policy_str)
          .value_or(AutoJoinPolicy::kPageScoped),
      cast_util::StringToEnum<DefaultActionPolicy>(default_action_policy_str)
          .value_or(DefaultActionPolicy::kCreateSession));
  cast_source->set_client_id(client_id);
  if (!broadcast_namespace.empty() && !encoded_broadcast_message.empty()) {
    cast_source->set_broadcast_request(BroadcastRequest(
        broadcast_namespace, DecodeURLComponent(encoded_broadcast_message)));
  }

  int launch_timeout_millis;
  if (base::StringToInt(launch_timeout_str, &launch_timeout_millis) &&
      launch_timeout_millis > 0) {
    cast_source->set_launch_timeout(
        base::TimeDelta::FromMilliseconds(launch_timeout_millis));
  }

  return cast_source;
}

std::unique_ptr<CastMediaSource> ParseCastUrl(const MediaSource::Id& source_id,
                                              const GURL& url) {
  std::string app_id = url.path();
  // App ID must be non-empty.
  if (app_id.empty())
    return nullptr;

  auto params{MakeQueryMap(url)};
  return CreateFromURLParams(
      source_id,
      {CastAppInfo(app_id, CastDeviceCapabilitiesFromString(
                               FindValueOr(params, "capabilities", "")))},
      FindValueOr(params, "autoJoinPolicy", ""),
      FindValueOr(params, "defaultActionPolicy", ""),
      FindValueOr(params, "clientId", ""),
      FindValueOr(params, "broadcastNamespace", ""),
      FindValueOr(params, "broadcastMessage", ""),
      FindValueOr(params, "launchTimeout", ""));
}

std::unique_ptr<CastMediaSource> ParseLegacyCastUrl(
    const MediaSource::Id& source_id,
    const GURL& url) {
  base::StringPairs params;
  base::SplitStringIntoKeyValuePairs(url.ref(), '=', '/', &params);
  for (auto& pair : params) {
    pair.second = net::UnescapeURLComponent(
        pair.second,
        net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
            net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
            net::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
  }

  // Legacy URLs can specify multiple apps.
  std::vector<std::string> app_id_params;
  for (const auto& param : params) {
    if (param.first == "__castAppId__")
      app_id_params.push_back(param.second);
  }

  std::vector<CastAppInfo> app_infos;
  for (const auto& app_id_param : app_id_params) {
    std::string app_id;
    std::string capabilities;
    auto cap_start_index = app_id_param.find('(');
    // If |cap_start_index| is |npos|, then this will return the entire string.
    app_id = app_id_param.substr(0, cap_start_index);
    if (cap_start_index != std::string::npos) {
      auto cap_end_index = app_id_param.find(')', cap_start_index);
      if (cap_end_index != std::string::npos &&
          cap_end_index > cap_start_index) {
        capabilities = app_id_param.substr(cap_start_index + 1,
                                           cap_end_index - cap_start_index - 1);
      }
    }

    if (app_id.empty())
      continue;

    CastAppInfo app_info(app_id,
                         CastDeviceCapabilitiesFromString(capabilities));

    app_infos.push_back(app_info);
  }

  if (app_infos.empty())
    return nullptr;

  return CreateFromURLParams(
      source_id, app_infos, FindValueOr(params, "__castAutoJoinPolicy__", ""),
      FindValueOr(params, "__castDefaultActionPolicy__", ""),
      FindValueOr(params, "__castClientId__", ""),
      FindValueOr(params, "__castBroadcastNamespace__", ""),
      FindValueOr(params, "__castBroadcastMessage__", ""),
      FindValueOr(params, "__castLaunchTimeout__", ""));
}

}  // namespace

bool IsAutoJoinAllowed(AutoJoinPolicy policy,
                       const url::Origin& origin1,
                       int tab_id1,
                       const url::Origin& origin2,
                       int tab_id2) {
  switch (policy) {
    case AutoJoinPolicy::kPageScoped:
      return false;
    case AutoJoinPolicy::kTabAndOriginScoped:
      return origin1 == origin2 && tab_id1 == tab_id2;
    case AutoJoinPolicy::kOriginScoped:
      return origin1 == origin2;
  }
}

CastAppInfo::CastAppInfo(
    const std::string& app_id,
    BitwiseOr<cast_channel::CastDeviceCapability> required_capabilities)
    : app_id(app_id), required_capabilities(required_capabilities) {}
CastAppInfo::~CastAppInfo() = default;

CastAppInfo::CastAppInfo(const CastAppInfo& other) = default;

// static
std::unique_ptr<CastMediaSource> CastMediaSource::FromMediaSource(
    const MediaSource& source) {
  if (source.IsTabMirroringSource())
    return CastMediaSourceForTabMirroring(source.id());

  if (source.IsDesktopMirroringSource())
    return CastMediaSourceForDesktopMirroring(source.id());

  const GURL& url = source.url();
  if (!url.is_valid())
    return nullptr;

  if (url.SchemeIs(kCastPresentationUrlScheme)) {
    return ParseCastUrl(source.id(), url);
  } else if (IsLegacyCastPresentationUrl(url)) {
    return ParseLegacyCastUrl(source.id(), url);
  } else if (url.SchemeIsHTTPOrHTTPS()) {
    // Arbitrary https URLs are supported via 1-UA mode which uses tab
    // mirroring.
    return CastMediaSourceForTabMirroring(source.id());
  }

  return nullptr;
}

// static
std::unique_ptr<CastMediaSource> CastMediaSource::FromMediaSourceId(
    const MediaSource::Id& source_id) {
  return FromMediaSource(MediaSource(source_id));
}

// static
std::unique_ptr<CastMediaSource> CastMediaSource::FromAppId(
    const std::string& app_id) {
  return FromMediaSourceId(kCastPresentationUrlScheme + (":" + app_id));
}

CastMediaSource::CastMediaSource(const MediaSource::Id& source_id,
                                 const std::vector<CastAppInfo>& app_infos,
                                 AutoJoinPolicy auto_join_policy,
                                 DefaultActionPolicy default_action_policy)
    : source_id_(source_id),
      app_infos_(app_infos),
      auto_join_policy_(auto_join_policy),
      default_action_policy_(default_action_policy) {}
CastMediaSource::CastMediaSource(const CastMediaSource& other) = default;
CastMediaSource::~CastMediaSource() = default;

bool CastMediaSource::ContainsApp(const std::string& app_id) const {
  for (const auto& info : app_infos_) {
    if (info.app_id == app_id)
      return true;
  }
  return false;
}

bool CastMediaSource::ContainsAnyAppFrom(
    const std::vector<std::string>& app_ids) const {
  return std::any_of(
      app_ids.begin(), app_ids.end(),
      [this](const std::string& app_id) { return ContainsApp(app_id); });
}

std::vector<std::string> CastMediaSource::GetAppIds() const {
  std::vector<std::string> app_ids;
  for (const auto& info : app_infos_)
    app_ids.push_back(info.app_id);

  return app_ids;
}

}  // namespace media_router
