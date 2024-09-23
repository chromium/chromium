// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/cast_media_source.h"

#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"
#include "net/base/url_util.h"
#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"
#include "url/gurl.h"
#include "url/url_util.h"

using cast_channel::CastDeviceCapability;
using cast_channel::ReceiverAppType;

namespace cast_util {

using media_router::AutoJoinPolicy;
using media_router::DefaultActionPolicy;

template <>
const EnumTable<AutoJoinPolicy>& EnumTable<AutoJoinPolicy>::GetInstance() {
  static const EnumTable<AutoJoinPolicy> kInstance(
      {
          {AutoJoinPolicy::kPageScoped, "page_scoped"},
          {AutoJoinPolicy::kTabAndOriginScoped, "tab_and_origin_scoped"},
          {AutoJoinPolicy::kOriginScoped, "origin_scoped"},
      },
      AutoJoinPolicy::kMaxValue);
  return kInstance;
}

template <>
const EnumTable<DefaultActionPolicy>&
EnumTable<DefaultActionPolicy>::GetInstance() {
  static const EnumTable<DefaultActionPolicy> kInstance(
      {
          {DefaultActionPolicy::kCreateSession, "create_session"},
          {DefaultActionPolicy::kCastThisTab, "cast_this_tab"},
      },
      DefaultActionPolicy::kMaxValue);
  return kInstance;
}

template <>
const EnumTable<CastDeviceCapability>&
EnumTable<CastDeviceCapability>::GetInstance() {
  static const EnumTable<CastDeviceCapability> kInstance(
      {
          {CastDeviceCapability::kVideoOut, "video_out"},
          {CastDeviceCapability::kVideoIn, "video_in"},
          {CastDeviceCapability::kAudioOut, "audio_out"},
          {CastDeviceCapability::kAudioIn, "audio_in"},
          {CastDeviceCapability::kDevMode, "dev_mode"},
          {CastDeviceCapability::kMultizoneGroup, "multizone_group"},
      },
      CastDeviceCapability::kMultizoneGroup);
  return kInstance;
}

template <>
const EnumTable<ReceiverAppType>& EnumTable<ReceiverAppType>::GetInstance() {
  static const EnumTable<ReceiverAppType> kInstance(
      {
          {ReceiverAppType::kOther, "OTHER"},
          {ReceiverAppType::kWeb, "WEB"},
          {ReceiverAppType::kAndroidTv, "ANDROID_TV"},
      },
      ReceiverAppType::kMaxValue);
  return kInstance;
}

}  // namespace cast_util

namespace media_router {

// The maximum length of presentation URL is 64KB.
constexpr int kMaxCastPresentationUrlLength = 64 * 1024;

namespace {

// A nonmember version of std::optional::value_or that works on pointers as
// well as instance of std::optional.
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
    result[std::string(query_it.GetKey())] = query_it.GetUnescapedValue();
  }
  return result;
}

// Converts a string containing a comma-separated list of capabilities into an
// EnumSet of CastDeviceCapability values.
CastDeviceCapabilitySet CastDeviceCapabilitiesFromString(std::string_view s) {
  CastDeviceCapabilitySet result;
  for (const auto& capability_str : base::SplitStringPiece(
           s, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    const auto capability =
        cast_util::StringToEnum<CastDeviceCapability>(capability_str);
    if (capability) {
      result.Put(*capability);
    } else {
      DLOG(ERROR) << "Unkown capability name: " << capability_str;
    }
  }
  return result;
}

std::vector<ReceiverAppType> SupportedAppTypesFromString(std::string_view s) {
  std::vector<ReceiverAppType> result;
  for (const auto& type_str : base::SplitStringPiece(
           s, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    const auto type = cast_util::StringToEnum<ReceiverAppType>(type_str);
    if (type) {
      result.push_back(*type);
    } else {
      DLOG(ERROR) << "Unkown supported app type: " << type_str;
    }
  }
  return result;
}

std::unique_ptr<CastMediaSource> CastMediaSourceForTabMirroring(
    const MediaSource::Id& source_id) {
  return std::make_unique<CastMediaSource>(
      source_id,
      std::vector<CastAppInfo>({CastAppInfo::ForCastStreaming(),
                                CastAppInfo::ForCastStreamingAudio()}));
}

std::unique_ptr<CastMediaSource> CastMediaSourceForDesktopMirroring(
    const MediaSource& source) {
  std::vector<CastAppInfo> app_infos;
  CastAppInfo audio_video_info = CastAppInfo::ForCastStreaming();
  if (source.IsDesktopSourceWithAudio()) {
    // Screen capture will result in audio and video streams.  Include
    // audio-only Cast Streaming receivers.
    app_infos.push_back(audio_video_info);
    app_infos.push_back(CastAppInfo::ForCastStreamingAudio());
  } else {
    // Screen capture will result in a video stream only.
    audio_video_info.required_capabilities.Remove(
        CastDeviceCapability::kAudioOut);
    app_infos.push_back(audio_video_info);
  }
  return std::make_unique<CastMediaSource>(source.id(), app_infos);
}

std::unique_ptr<CastMediaSource> CastMediaSourceForRemotePlayback(
    const MediaSource& source) {
  return CastMediaSourceForTabMirroring(source.id());
}

// The logic shared by ParseCastUrl() and ParseLegacyCastUrl().
std::unique_ptr<CastMediaSource> CreateFromURLParams(
    const MediaSource::Id& source_id,
    const std::vector<CastAppInfo>& app_infos,
    const std::string& auto_join_policy_str,
    const std::string& default_action_policy_str,
    const std::string& client_id,
    const std::string& launch_timeout_str,
    const std::string& target_playout_delay_millis_str,
    const std::string& audio_capture_str,
    const std::vector<ReceiverAppType>& supported_app_types,
    const std::string& app_params,
    const std::string& invisible_sender) {
  if (app_infos.empty())
    return nullptr;

  auto cast_source = std::make_unique<CastMediaSource>(
      source_id, app_infos,
      cast_util::StringToEnum<AutoJoinPolicy>(auto_join_policy_str)
          .value_or(AutoJoinPolicy::kPageScoped),
      cast_util::StringToEnum<DefaultActionPolicy>(default_action_policy_str)
          .value_or(DefaultActionPolicy::kCreateSession));
  cast_source->set_client_id(client_id);

  int launch_timeout_millis = 0;
  if (base::StringToInt(launch_timeout_str, &launch_timeout_millis) &&
      launch_timeout_millis > 0) {
    cast_source->set_launch_timeout(base::Milliseconds(launch_timeout_millis));
  }

  int target_playout_delay_millis = 0;
  if (base::StringToInt(target_playout_delay_millis_str,
                        &target_playout_delay_millis) &&
      target_playout_delay_millis > 0) {
    cast_source->set_target_playout_delay(
        base::Milliseconds(target_playout_delay_millis));
  }

  if (audio_capture_str == "0")
    cast_source->set_site_requested_audio_capture(false);

  if (!supported_app_types.empty())
    cast_source->set_supported_app_types(supported_app_types);
  cast_source->set_app_params(app_params);

  if (invisible_sender == "true") {
    cast_source->set_connection_type(
        cast_channel::VirtualConnectionType::kInvisible);
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
      FindValueOr(params, "launchTimeout", ""),
      FindValueOr(params, "streamingTargetPlayoutDelayMillis", ""),
      FindValueOr(params, "streamingCaptureAudio", ""),
      SupportedAppTypesFromString(FindValueOr(params, "supportedAppTypes", "")),
      FindValueOr(params, "appParams", ""),
      FindValueOr(params, "invisibleSender", ""));
}

std::unique_ptr<CastMediaSource> ParseLegacyCastUrl(
    const MediaSource::Id& source_id,
    const GURL& url) {
  base::StringPairs params;
  base::SplitStringIntoKeyValuePairs(url.ref(), '=', '/', &params);
  for (auto& pair : params) {
    pair.second = base::UnescapeURLComponent(
        pair.second,
        base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
            base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
            base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
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
      FindValueOr(params, "__castLaunchTimeout__", ""),
      /* target_playout_delay_millis_str */ "",
      /* audio_capture */ "",
      /* supported_app_types */ {},
      /* appParams */ "",
      /* invisibleSender */ "");
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

bool IsSiteInitiatedMirroringSource(const MediaSource::Id& source_id) {
  // A Cast SDK enabled website (e.g. Google Slides) may use the mirroring app
  // ID rather than the tab mirroring URN.
  return base::StartsWith(
      source_id,
      base::StrCat(
          {"cast:", openscreen::cast::GetCastStreamingAudioVideoAppId()}),
      base::CompareCase::SENSITIVE);
}

CastAppInfo::CastAppInfo(const std::string& app_id,
                         CastDeviceCapabilitySet required_capabilities)
    : app_id(app_id), required_capabilities(required_capabilities) {}

CastAppInfo::~CastAppInfo() = default;

CastAppInfo::CastAppInfo(const CastAppInfo& other) = default;

// static
CastAppInfo CastAppInfo::ForCastStreaming() {
  return CastAppInfo(
      openscreen::cast::GetCastStreamingAudioVideoAppId(),
      {CastDeviceCapability::kVideoOut, CastDeviceCapability::kAudioOut});
}

// static
CastAppInfo CastAppInfo::ForCastStreamingAudio() {
  return CastAppInfo(openscreen::cast::GetCastStreamingAudioOnlyAppId(),
                     {CastDeviceCapability::kAudioOut});
}

// static
std::unique_ptr<CastMediaSource> CastMediaSource::FromMediaSource(
    const MediaSource& source) {
  if (source.IsTabMirroringSource())
    return CastMediaSourceForTabMirroring(source.id());

  if (source.IsDesktopMirroringSource())
    return CastMediaSourceForDesktopMirroring(source);

  if (source.IsRemotePlaybackSource())
    return CastMediaSourceForRemotePlayback(source);

  const GURL& url = source.url();

  if (!url.is_valid() || url.spec().length() > kMaxCastPresentationUrlLength)
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

// static
std::unique_ptr<CastMediaSource> CastMediaSource::ForSiteInitiatedMirroring() {
  return FromAppId(openscreen::cast::GetCastStreamingAudioVideoAppId());
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
  return base::ranges::any_of(app_ids, [this](const std::string& app_id) {
    return ContainsApp(app_id);
  });
}

bool CastMediaSource::ContainsStreamingApp() const {
  return ContainsAnyAppFrom(openscreen::cast::GetCastStreamingAppIds());
}

std::vector<std::string> CastMediaSource::GetAppIds() const {
  std::vector<std::string> app_ids;
  for (const auto& info : app_infos_)
    app_ids.push_back(info.app_id);

  return app_ids;
}

bool CastMediaSource::ProvidesStreamingAudioCapture() const {
  if (!site_requested_audio_capture_) {
    return false;
  }
  for (const auto& info : app_infos_) {
    if (openscreen::cast::IsCastStreamingAppId(info.app_id) &&
        info.required_capabilities.Has(CastDeviceCapability::kAudioOut)) {
      return true;
    }
  }
  return false;
}

void CastMediaSource::set_supported_app_types(
    const std::vector<ReceiverAppType>& types) {
  DCHECK(!types.empty());
  DCHECK(base::Contains(types, ReceiverAppType::kWeb));
  supported_app_types_ = types;
}

}  // namespace media_router
