// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/media_source.h"

#include <array>
#include <cstdio>
#include <ostream>
#include <string>
#include <string_view>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/platform/modules/remoteplayback/remote_playback_source.h"
#include "url/gurl.h"

namespace media_router {

namespace {

// Prefixes used to format and detect various protocols' media source URNs.
// See: https://www.ietf.org/rfc/rfc3406.txt
constexpr char kAnyTabMediaUrn[] = "urn:x-org.chromium.media:source:tab:*";
constexpr char kTabMediaUrnFormat[] = "urn:x-org.chromium.media:source:tab:%d";
constexpr std::string_view kDesktopMediaUrnPrefix =
    "urn:x-org.chromium.media:source:desktop:";
// WARNING: If more desktop URN parameters are added in the future, the parsing
// code will have to be smarter!
constexpr std::string_view kDesktopMediaUrnAudioParam = "?with_audio=true";
constexpr std::string_view kUnchosenDesktopMediaUrn =
    "urn:x-org.chromium.media:source:desktop";
constexpr std::string_view kUnchosenDesktopWithAudioMediaUrn =
    "urn:x-org.chromium.media:source:desktop:?with_audio=true";

// List of non-http(s) schemes that are allowed in a Presentation URL.
constexpr std::array<const char* const, 5> kAllowedSchemes{
    {kCastPresentationUrlScheme, kCastDialPresentationUrlScheme,
     kDialPresentationUrlScheme, blink::kRemotePlaybackPresentationUrlScheme,
     "test"}};

bool IsSchemeAllowed(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() ||
         base::ranges::any_of(
             kAllowedSchemes,
             [&url](const char* const scheme) { return url.SchemeIs(scheme); });
}

bool IsSystemAudioCaptureSupported() {
  if (!media::IsSystemLoopbackCaptureSupported()) {
    return false;
  }
#if BUILDFLAG(IS_LINUX)
  return base::FeatureList::IsEnabled(media::kPulseaudioLoopbackForCast);
#else
  return true;
#endif  // BUILDFLAG(IS_LINUX)
}

}  // namespace

bool IsLegacyCastPresentationUrl(const GURL& url) {
  return base::StartsWith(url.spec(), kLegacyCastPresentationUrlPrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsValidPresentationUrl(const GURL& url) {
  return url.is_valid() && IsSchemeAllowed(url);
}

bool IsValidStandardPresentationSource(const std::string& media_source) {
  const GURL source_url(media_source);
  return source_url.is_valid() && source_url.SchemeIsHTTPOrHTTPS() &&
         !base::StartsWith(source_url.spec(), kLegacyCastPresentationUrlPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

bool IsAutoJoinPresentationId(const std::string& presentation_id) {
  return presentation_id == kAutoJoinPresentationId;
}

MediaSource::MediaSource(const MediaSource::Id& source_id) : id_(source_id) {
  GURL url(source_id);
  if (IsValidPresentationUrl(url))
    url_ = url;
}

MediaSource::MediaSource(const GURL& presentation_url)
    : id_(presentation_url.spec()), url_(presentation_url) {}

MediaSource::~MediaSource() = default;

// static
MediaSource MediaSource::ForAnyTab() {
  return MediaSource(std::string(kAnyTabMediaUrn));
}

// static
MediaSource MediaSource::ForTab(int tab_id) {
  // Ideally we shouldn't allow -1 as a tab ID, but in unit tests, a tab ID of
  // -1 can show up when this function is called from
  // CastHandler::StartObservingForSinks() because SessionTabHelper::IdForTab
  // can return -1.
  DCHECK_GE(tab_id, -1);
  return MediaSource(base::StringPrintf(kTabMediaUrnFormat, tab_id));
}

// static
MediaSource MediaSource::ForPresentationUrl(const GURL& presentation_url) {
  return MediaSource(presentation_url);
}

// static
MediaSource MediaSource::ForRemotePlayback(int tab_id,
                                           media::VideoCodec video_codec,
                                           media::AudioCodec audio_codec) {
  return MediaSource(
      base::StringPrintf(blink::kRemotePlaybackDesktopUrlFormat,
                         media::GetCodecName(video_codec).c_str(),
                         media::GetCodecName(audio_codec).c_str(), tab_id));
}

// static
MediaSource MediaSource::ForDesktop(const std::string& desktop_media_id,
                                    bool with_audio) {
  DCHECK(!desktop_media_id.empty());
  std::string id = std::string(kDesktopMediaUrnPrefix) + desktop_media_id;
  if (with_audio) {
    id += std::string(kDesktopMediaUrnAudioParam);
  }
  return MediaSource(id);
}

// static
MediaSource MediaSource::ForUnchosenDesktop() {
  return IsSystemAudioCaptureSupported() &&
                 base::FeatureList::IsEnabled(
                     media::kCastLoopbackAudioToAudioReceivers)
             ? MediaSource(std::string(kUnchosenDesktopWithAudioMediaUrn))
             : MediaSource(std::string(kUnchosenDesktopMediaUrn));
}

bool MediaSource::IsTabMirroringSource() const {
  return id() == kAnyTabMediaUrn || TabId().has_value();
}

bool MediaSource::IsDesktopMirroringSource() const {
  return id() == kUnchosenDesktopMediaUrn ||
         id() == kUnchosenDesktopWithAudioMediaUrn ||
         base::StartsWith(id(), kDesktopMediaUrnPrefix,
                          base::CompareCase::SENSITIVE);
}

bool MediaSource::IsCastPresentationUrl() const {
  return url_.SchemeIs(kCastPresentationUrlScheme) ||
         IsLegacyCastPresentationUrl(url_);
}

bool MediaSource::IsRemotePlaybackSource() const {
  return url_.SchemeIs(kRemotePlaybackPresentationUrlScheme);
}

std::optional<int> MediaSource::TabId() const {
  int tab_id;
  if (sscanf(id_.c_str(), kTabMediaUrnFormat, &tab_id) != 1) {
    return std::nullopt;
  }
  return tab_id;
}

std::optional<int> MediaSource::TabIdFromRemotePlaybackSource() const {
  if (!IsRemotePlaybackSource()) {
    return std::nullopt;
  }

  std::string tab_id_str;
  if (!net::GetValueForKeyInQuery(url(), "tab_id", &tab_id_str)) {
    return std::nullopt;
  }

  int tab_id;
  if (!base::StringToInt(tab_id_str, &tab_id)) {
    return std::nullopt;
  }
  return tab_id;
}

std::optional<std::string> MediaSource::DesktopStreamId() const {
  if (base::StartsWith(id_, kDesktopMediaUrnPrefix,
                       base::CompareCase::SENSITIVE)) {
    const auto begin = id_.begin() + kDesktopMediaUrnPrefix.size();
    auto end = id_.end();
    if (base::EndsWith(id_, kDesktopMediaUrnAudioParam,
                       base::CompareCase::SENSITIVE)) {
      end -= kDesktopMediaUrnAudioParam.size();
    }
    return std::string(begin, end);
  }
  return std::nullopt;
}

bool MediaSource::IsDesktopSourceWithAudio() const {
  return base::StartsWith(id_, kDesktopMediaUrnPrefix,
                          base::CompareCase::SENSITIVE) &&
         base::EndsWith(id_, kDesktopMediaUrnAudioParam,
                        base::CompareCase::SENSITIVE);
}

bool MediaSource::IsDialSource() const {
  return url_.SchemeIs(kCastDialPresentationUrlScheme);
}

std::string MediaSource::AppNameFromDialSource() const {
  return IsDialSource() ? url_.path() : "";
}

std::string MediaSource::TruncateForLogging(size_t max_length) const {
  const std::string origin = url_.DeprecatedGetOriginAsURL().spec();
  if (!origin.empty()) {
    return origin.substr(0, max_length);
  }
  // TODO(takumif): Keep the query string by redacting PII. The query string
  // may contain info useful for debugging such as the required capabilities.
  const size_t query_start_index = id_.find("?");
  const size_t length =
      query_start_index == std::string::npos ? max_length : query_start_index;
  return id_.substr(0, length);
}

void MediaSource::AppendTabIdToRemotePlaybackUrlQuery(int tab_id) {
  if (url_.is_empty()) {
    return;
  }

  GURL::Replacements replacements;
  std::string tab_id_query = base::StringPrintf("tab_id=%d", tab_id);
  std::string new_query =
      (url_.has_query() ? url_.query() + "&" : "") + tab_id_query;
  replacements.SetQueryStr(new_query);
  url_ = url_.ReplaceComponents(replacements);
  id_ = url_.spec();
}

}  // namespace media_router
