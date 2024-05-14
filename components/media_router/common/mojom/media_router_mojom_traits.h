// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_MOJOM_MEDIA_ROUTER_MOJOM_TRAITS_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_MOJOM_MEDIA_ROUTER_MOJOM_TRAITS_H_

#include <string>

#include "base/notreached.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/mojom/media_router.mojom-shared.h"
#include "components/media_router/common/route_request_result.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"
#include "net/base/ip_endpoint.h"

namespace mojo {

// Issue

template <>
struct EnumTraits<media_router::mojom::Issue_Severity,
                  media_router::IssueInfo::Severity> {
  static media_router::mojom::Issue_Severity ToMojom(
      media_router::IssueInfo::Severity severity) {
    switch (severity) {
      case media_router::IssueInfo::Severity::WARNING:
        return media_router::mojom::Issue_Severity::WARNING;
      case media_router::IssueInfo::Severity::NOTIFICATION:
        return media_router::mojom::Issue_Severity::NOTIFICATION;
    }
    NOTREACHED_IN_MIGRATION()
        << "Unknown issue severity " << static_cast<int>(severity);
    return media_router::mojom::Issue_Severity::WARNING;
  }

  static bool FromMojom(media_router::mojom::Issue_Severity input,
                        media_router::IssueInfo::Severity* output) {
    switch (input) {
      case media_router::mojom::Issue_Severity::WARNING:
        *output = media_router::IssueInfo::Severity::WARNING;
        return true;
      case media_router::mojom::Issue_Severity::NOTIFICATION:
        *output = media_router::IssueInfo::Severity::NOTIFICATION;
        return true;
    }
    return false;
  }
};

template <>
struct UnionTraits<media_router::mojom::MediaSinkExtraDataDataView,
                   media_router::MediaSinkInternal> {
  static media_router::mojom::MediaSinkExtraDataDataView::Tag GetTag(
      const media_router::MediaSinkInternal& sink);

  static bool IsNull(const media_router::MediaSinkInternal& sink) {
    return !sink.is_cast_sink() && !sink.is_dial_sink();
  }

  static void SetToNull(media_router::MediaSinkInternal* out) {}

  static const media_router::DialSinkExtraData& dial_media_sink(
      const media_router::MediaSinkInternal& sink) {
    return sink.dial_data();
  }

  static const media_router::CastSinkExtraData& cast_media_sink(
      const media_router::MediaSinkInternal& sink) {
    return sink.cast_data();
  }

  static bool Read(media_router::mojom::MediaSinkExtraDataDataView data,
                   media_router::MediaSinkInternal* out);
};

template <>
struct StructTraits<media_router::mojom::DialMediaSinkDataView,
                    media_router::DialSinkExtraData> {
  static const std::string& model_name(
      const media_router::DialSinkExtraData& extra_data) {
    return extra_data.model_name;
  }

  static const net::IPAddress& ip_address(
      const media_router::DialSinkExtraData& extra_data) {
    return extra_data.ip_address;
  }

  static const GURL& app_url(
      const media_router::DialSinkExtraData& extra_data) {
    return extra_data.app_url;
  }

  static bool Read(media_router::mojom::DialMediaSinkDataView data,
                   media_router::DialSinkExtraData* out);
};

template <>
struct StructTraits<media_router::mojom::CastMediaSinkDataView,
                    media_router::CastSinkExtraData> {
  static const std::string& model_name(
      const media_router::CastSinkExtraData& extra_data) {
    return extra_data.model_name;
  }

  static const net::IPEndPoint& ip_endpoint(
      const media_router::CastSinkExtraData& extra_data) {
    return extra_data.ip_endpoint;
  }

  static uint64_t capabilities(
      const media_router::CastSinkExtraData& extra_data) {
    return extra_data.capabilities.ToEnumBitmask();
  }

  static int32_t cast_channel_id(
      const media_router::CastSinkExtraData& extra_data) {
    return extra_data.cast_channel_id;
  }

  static bool Read(media_router::mojom::CastMediaSinkDataView data,
                   media_router::CastSinkExtraData* out);
};

template <>
struct StructTraits<media_router::mojom::IssueDataView,
                    media_router::IssueInfo> {
  static bool Read(media_router::mojom::IssueDataView data,
                   media_router::IssueInfo* out);

  static const std::string& route_id(const media_router::IssueInfo& issue) {
    return issue.route_id;
  }

  static const std::string& sink_id(const media_router::IssueInfo& issue) {
    return issue.sink_id;
  }

  static media_router::IssueInfo::Severity severity(
      const media_router::IssueInfo& issue) {
    return issue.severity;
  }

  static const std::string& title(const media_router::IssueInfo& issue) {
    return issue.title;
  }

  static const std::string& message(const media_router::IssueInfo& issue) {
    return issue.message;
  }
};

// MediaSink

template <>
struct EnumTraits<media_router::mojom::SinkIconType,
                  media_router::SinkIconType> {
  static media_router::mojom::SinkIconType ToMojom(
      media_router::SinkIconType icon_type) {
    switch (icon_type) {
      case media_router::SinkIconType::CAST:
        return media_router::mojom::SinkIconType::CAST;
      case media_router::SinkIconType::CAST_AUDIO_GROUP:
        return media_router::mojom::SinkIconType::CAST_AUDIO_GROUP;
      case media_router::SinkIconType::CAST_AUDIO:
        return media_router::mojom::SinkIconType::CAST_AUDIO;
      case media_router::SinkIconType::WIRED_DISPLAY:
        return media_router::mojom::SinkIconType::WIRED_DISPLAY;
      case media_router::SinkIconType::GENERIC:
        return media_router::mojom::SinkIconType::GENERIC;
      case media_router::SinkIconType::TOTAL_COUNT:
        break;
    }
    NOTREACHED_IN_MIGRATION()
        << "Unknown sink icon type " << static_cast<int>(icon_type);
    return media_router::mojom::SinkIconType::GENERIC;
  }

  static bool FromMojom(media_router::mojom::SinkIconType input,
                        media_router::SinkIconType* output) {
    switch (input) {
      case media_router::mojom::SinkIconType::CAST:
        *output = media_router::SinkIconType::CAST;
        return true;
      case media_router::mojom::SinkIconType::CAST_AUDIO_GROUP:
        *output = media_router::SinkIconType::CAST_AUDIO_GROUP;
        return true;
      case media_router::mojom::SinkIconType::CAST_AUDIO:
        *output = media_router::SinkIconType::CAST_AUDIO;
        return true;
      case media_router::mojom::SinkIconType::WIRED_DISPLAY:
        *output = media_router::SinkIconType::WIRED_DISPLAY;
        return true;
      case media_router::mojom::SinkIconType::GENERIC:
        *output = media_router::SinkIconType::GENERIC;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<media_router::mojom::MediaSinkDataView,
                    media_router::MediaSinkInternal> {
  static bool Read(media_router::mojom::MediaSinkDataView data,
                   media_router::MediaSinkInternal* out);

  static const std::string& sink_id(
      const media_router::MediaSinkInternal& sink_internal) {
    return sink_internal.sink().id();
  }

  static const std::string& name(
      const media_router::MediaSinkInternal& sink_internal) {
    return sink_internal.sink().name();
  }

  static media_router::SinkIconType icon_type(
      const media_router::MediaSinkInternal& sink_internal) {
    return sink_internal.sink().icon_type();
  }

  static media_router::mojom::MediaRouteProviderId provider_id(
      const media_router::MediaSinkInternal& sink_internal) {
    return sink_internal.sink().provider_id();
  }

  static const media_router::MediaSinkInternal& extra_data(
      const media_router::MediaSinkInternal& sink_internal) {
    return sink_internal;
  }
};

// MediaRoute

template <>
struct EnumTraits<media_router::mojom::RouteControllerType,
                  media_router::RouteControllerType> {
  static media_router::mojom::RouteControllerType ToMojom(
      media_router::RouteControllerType controller_type) {
    switch (controller_type) {
      case media_router::RouteControllerType::kNone:
        return media_router::mojom::RouteControllerType::kNone;
      case media_router::RouteControllerType::kGeneric:
        return media_router::mojom::RouteControllerType::kGeneric;
      case media_router::RouteControllerType::kMirroring:
        return media_router::mojom::RouteControllerType::kMirroring;
    }
    NOTREACHED_IN_MIGRATION()
        << "Unknown controller type " << static_cast<int>(controller_type);
    return media_router::mojom::RouteControllerType::kNone;
  }

  static bool FromMojom(media_router::mojom::RouteControllerType input,
                        media_router::RouteControllerType* output) {
    switch (input) {
      case media_router::mojom::RouteControllerType::kNone:
        *output = media_router::RouteControllerType::kNone;
        return true;
      case media_router::mojom::RouteControllerType::kGeneric:
        *output = media_router::RouteControllerType::kGeneric;
        return true;
      case media_router::mojom::RouteControllerType::kMirroring:
        *output = media_router::RouteControllerType::kMirroring;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<media_router::mojom::MediaRouteDataView,
                    media_router::MediaRoute> {
  static bool Read(media_router::mojom::MediaRouteDataView data,
                   media_router::MediaRoute* out);

  static const std::string& media_route_id(
      const media_router::MediaRoute& route) {
    return route.media_route_id();
  }

  static const std::string& presentation_id(
      const media_router::MediaRoute& route) {
    return route.presentation_id();
  }

  static mojo::OptionalAsPointer<const std::string> media_source(
      const media_router::MediaRoute& route) {
    // TODO(imcheng): If we ever convert from C++ to Mojo outside of unit tests,
    // it would be better to make the |media_source_| field on MediaRoute a
    // std::optional<MediaSource::Id> instead so it can be returned directly
    // here.
    return mojo::OptionalAsPointer(route.media_source().id().empty()
                                       ? nullptr
                                       : &route.media_source().id());
  }

  static const std::string& media_sink_id(
      const media_router::MediaRoute& route) {
    return route.media_sink_id();
  }

  static const std::string& media_sink_name(
      const media_router::MediaRoute& route) {
    return route.media_sink_name();
  }

  static const std::string& description(const media_router::MediaRoute& route) {
    return route.description();
  }

  static bool is_local(const media_router::MediaRoute& route) {
    return route.is_local();
  }

  static media_router::RouteControllerType controller_type(
      const media_router::MediaRoute& route) {
    return route.controller_type();
  }

  static bool is_local_presentation(const media_router::MediaRoute& route) {
    return route.is_local_presentation();
  }

  static bool is_connecting(const media_router::MediaRoute& route) {
    return route.is_connecting();
  }
};

}  // namespace mojo

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_MOJOM_MEDIA_ROUTER_MOJOM_TRAITS_H_
