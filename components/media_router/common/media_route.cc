// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/media_route.h"

#include <ostream>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/media_router/common/media_source.h"

namespace media_router {

constexpr char kRouteIdPrefix[] = "urn:x-org.chromium:media:route:";

namespace {

bool IsValidMediaRouteId(const MediaRoute::Id route_id) {
  if (!base::StartsWith(route_id, kRouteIdPrefix, base::CompareCase::SENSITIVE))
    return false;
  // return false if there are not at least two slashes in |route_id|.
  size_t pos;
  return ((pos = route_id.find("/")) != std::string::npos &&
          (pos = route_id.find("/", pos + 1)) != std::string::npos);
}

}  // namespace

// static
MediaRoute::Id MediaRoute::GetMediaRouteId(const std::string& presentation_id,
                                           const MediaSink::Id& sink_id,
                                           const MediaSource& source) {
  // TODO(crbug.com/40090609): Can the route ID just be the presentation
  // id?
  return base::StringPrintf("%s%s/%s/%s", kRouteIdPrefix,
                            presentation_id.c_str(), sink_id.c_str(),
                            source.id().c_str());
}

// static
std::string MediaRoute::GetPresentationIdFromMediaRouteId(
    const MediaRoute::Id route_id) {
  if (!IsValidMediaRouteId(route_id)) {
    return "";
  }
  return route_id.substr(strlen(kRouteIdPrefix),
                         route_id.find("/") - strlen(kRouteIdPrefix));
}

// static
std::string MediaRoute::GetSinkIdFromMediaRouteId(
    const MediaRoute::Id route_id) {
  if (!IsValidMediaRouteId(route_id)) {
    return "";
  }
  auto begin = route_id.find("/");
  auto end = route_id.find("/", begin + 1);
  return route_id.substr(begin + 1, (end - begin - 1));
}

// static
std::string MediaRoute::GetMediaSourceIdFromMediaRouteId(
    const MediaRoute::Id route_id) {
  if (!IsValidMediaRouteId(route_id)) {
    return "";
  }
  auto pos = route_id.find("/");
  pos = route_id.find("/", pos + 1);
  return route_id.substr(pos + 1);
}

MediaRoute::MediaRoute(const MediaRoute::Id& media_route_id,
                       const MediaSource& media_source,
                       const MediaSink::Id& media_sink_id,
                       const std::string& description,
                       bool is_local)
    : media_route_id_(media_route_id),
      media_source_(media_source),
      media_sink_id_(media_sink_id),
      description_(description),
      is_local_(is_local) {}

MediaRoute::MediaRoute() : media_source_("") {}
MediaRoute::MediaRoute(const MediaRoute&) = default;
MediaRoute& MediaRoute::operator=(const MediaRoute&) = default;
MediaRoute::MediaRoute(MediaRoute&&) = default;
MediaRoute& MediaRoute::operator=(MediaRoute&&) = default;

MediaRoute::~MediaRoute() = default;

bool MediaRoute::IsLocalMirroringRoute() const {
  return is_local_ && (media_source_.IsTabMirroringSource() ||
                       media_source_.IsDesktopMirroringSource() ||
                       controller_type_ == RouteControllerType::kMirroring);
}

bool MediaRoute::operator==(const MediaRoute& other) const {
  return media_route_id_ == other.media_route_id_ &&
         presentation_id_ == other.presentation_id_ &&
         media_source_ == other.media_source_ &&
         media_sink_id_ == other.media_sink_id_ &&
         media_sink_name_ == other.media_sink_name_ &&
         description_ == other.description_ && is_local_ == other.is_local_ &&
         controller_type_ == other.controller_type_ &&
         is_local_presentation_ == other.is_local_presentation_ &&
         is_connecting_ == other.is_connecting_;
}

std::ostream& operator<<(std::ostream& stream, const MediaRoute& route) {
  return stream << "MediaRoute{id=" << route.media_route_id_
                << ",source=" << route.media_source_.id()
                << ",sink=" << route.media_sink_id_ << "}";
}

}  // namespace media_router
