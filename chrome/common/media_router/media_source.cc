// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/media_source.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <ostream>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/media_router/media_source.h"
#include "url/gurl.h"

namespace media_router {

namespace {

// Prefixes used to format and detect various protocols' media source URNs.
// See: https://www.ietf.org/rfc/rfc3406.txt
constexpr char kTabMediaUrnFormat[] = "urn:x-org.chromium.media:source:tab:%d";
constexpr base::StringPiece kDesktopMediaUrnPrefix =
    "urn:x-org.chromium.media:source:desktop:";
constexpr base::StringPiece kUnknownDesktopMediaUrn =
    "urn:x-org.chromium.media:source:desktop";
constexpr char kTabRemotingUrnFormat[] =
    "urn:x-org.chromium.media:source:tab_content_remoting:%d";

// List of non-http(s) schemes that are allowed in a Presentation URL.
constexpr std::array<const char* const, 5> kAllowedSchemes{
    {kCastPresentationUrlScheme, kCastDialPresentationUrlScheme,
     kDialPresentationUrlScheme, kRemotePlaybackPresentationUrlScheme, "test"}};

bool IsSchemeAllowed(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() ||
         std::any_of(
             kAllowedSchemes.begin(), kAllowedSchemes.end(),
             [&url](const char* const scheme) { return url.SchemeIs(scheme); });
}

}  // namespace

bool IsLegacyCastPresentationUrl(const GURL& url) {
  return base::StartsWith(url.spec(), kLegacyCastPresentationUrlPrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsValidPresentationUrl(const GURL& url) {
  return url.is_valid() && IsSchemeAllowed(url);
}

bool IsAutoJoinPresentationId(const std::string& presentation_id) {
  return presentation_id == kAutoJoinPresentationId;
}

MediaSource::MediaSource() = default;

MediaSource::MediaSource(const MediaSource::Id& source_id) : id_(source_id) {
  GURL url(source_id);
  if (IsValidPresentationUrl(url))
    url_ = url;
}

MediaSource::MediaSource(const GURL& presentation_url)
    : id_(presentation_url.spec()), url_(presentation_url) {}

MediaSource::~MediaSource() = default;

// static
MediaSource MediaSource::ForTab(int tab_id) {
  return MediaSource(base::StringPrintf(kTabMediaUrnFormat, tab_id));
}

// static
MediaSource MediaSource::ForTabContentRemoting(int tab_id) {
  return MediaSource(base::StringPrintf(kTabRemotingUrnFormat, tab_id));
}

// static
MediaSource MediaSource::ForDesktop(const std::string& desktop_media_id) {
  return MediaSource(kDesktopMediaUrnPrefix.as_string() + desktop_media_id);
}

// static
MediaSource MediaSource::ForDesktop() {
  return MediaSource(kUnknownDesktopMediaUrn.as_string());
}

// static
MediaSource MediaSource::ForPresentationUrl(const GURL& presentation_url) {
  return MediaSource(presentation_url);
}

bool MediaSource::IsDesktopMirroringSource() const {
  return id() == kUnknownDesktopMediaUrn ||
         base::StartsWith(id(), kDesktopMediaUrnPrefix,
                          base::CompareCase::SENSITIVE);
}

bool MediaSource::IsTabMirroringSource() const {
  int tab_id;
  return std::sscanf(id_.c_str(), kTabMediaUrnFormat, &tab_id) == 1 &&
         tab_id > 0;
}

bool MediaSource::IsMirroringSource() const {
  return IsDesktopMirroringSource() || IsTabMirroringSource();
}

bool MediaSource::IsCastPresentationUrl() const {
  return url_.SchemeIs(kCastPresentationUrlScheme) ||
         IsLegacyCastPresentationUrl(url_);
}

int MediaSource::TabId() const {
  int tab_id;
  if (sscanf(id_.c_str(), kTabMediaUrnFormat, &tab_id) == 1)
    return tab_id;
  else if (sscanf(id_.c_str(), kTabRemotingUrnFormat, &tab_id) == 1)
    return tab_id;
  else
    return -1;
}

base::Optional<std::string> MediaSource::DesktopStreamId() const {
  if (base::StartsWith(id_, kDesktopMediaUrnPrefix,
                       base::CompareCase::SENSITIVE)) {
    return std::string(id_.begin() + kDesktopMediaUrnPrefix.size(), id_.end());
  }
  return base::nullopt;
}

bool MediaSource::IsValid() const {
  return TabId() > 0 || IsDesktopMirroringSource() ||
         IsValidPresentationUrl(GURL(id_));
}

bool MediaSource::IsDialSource() const {
  return url_.SchemeIs(kCastDialPresentationUrlScheme);
}

std::string MediaSource::AppNameFromDialSource() const {
  return IsDialSource() ? url_.path() : "";
}

}  // namespace media_router
