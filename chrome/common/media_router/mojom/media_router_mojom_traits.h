// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_MOJOM_MEDIA_ROUTER_MOJOM_TRAITS_H_
#define CHROME_COMMON_MEDIA_ROUTER_MOJOM_MEDIA_ROUTER_MOJOM_TRAITS_H_

#include <string>

#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "chrome/common/media_router/route_request_result.h"
#include "net/base/ip_endpoint.h"

namespace mojo {

// Issue

template <>
struct EnumTraits<media_router::mojom::Issue::ActionType,
                  media_router::IssueInfo::Action> {
  static media_router::mojom::Issue::ActionType ToMojom(
      media_router::IssueInfo::Action action) {
    switch (action) {
      case media_router::IssueInfo::Action::DISMISS:
        return media_router::mojom::Issue::ActionType::DISMISS;
      case media_router::IssueInfo::Action::LEARN_MORE:
        return media_router::mojom::Issue::ActionType::LEARN_MORE;
    }
    NOTREACHED() << "Unknown issue action type " << static_cast<int>(action);
    return media_router::mojom::Issue::ActionType::DISMISS;
  }

  static bool FromMojom(media_router::mojom::Issue::ActionType input,
                        media_router::IssueInfo::Action* output) {
    switch (input) {
      case media_router::mojom::Issue::ActionType::DISMISS:
        *output = media_router::IssueInfo::Action::DISMISS;
        return true;
      case media_router::mojom::Issue::ActionType::LEARN_MORE:
        *output = media_router::IssueInfo::Action::LEARN_MORE;
        return true;
    }
    return false;
  }
};

template <>
struct EnumTraits<media_router::mojom::Issue::Severity,
                  media_router::IssueInfo::Severity> {
  static media_router::mojom::Issue::Severity ToMojom(
      media_router::IssueInfo::Severity severity) {
    switch (severity) {
      case media_router::IssueInfo::Severity::FATAL:
        return media_router::mojom::Issue::Severity::FATAL;
      case media_router::IssueInfo::Severity::WARNING:
        return media_router::mojom::Issue::Severity::WARNING;
      case media_router::IssueInfo::Severity::NOTIFICATION:
        return media_router::mojom::Issue::Severity::NOTIFICATION;
    }
    NOTREACHED() << "Unknown issue severity " << static_cast<int>(severity);
    return media_router::mojom::Issue::Severity::WARNING;
  }

  static bool FromMojom(media_router::mojom::Issue::Severity input,
                        media_router::IssueInfo::Severity* output) {
    switch (input) {
      case media_router::mojom::Issue::Severity::FATAL:
        *output = media_router::IssueInfo::Severity::FATAL;
        return true;
      case media_router::mojom::Issue::Severity::WARNING:
        *output = media_router::IssueInfo::Severity::WARNING;
        return true;
      case media_router::mojom::Issue::Severity::NOTIFICATION:
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

  static uint8_t capabilities(
      const media_router::CastSinkExtraData& extra_data) {
    return extra_data.capabilities;
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

  static bool is_blocking(const media_router::IssueInfo& issue) {
    return issue.is_blocking;
  }

  static const std::string& title(const media_router::IssueInfo& issue) {
    return issue.title;
  }

  static const std::string& message(const media_router::IssueInfo& issue) {
    return issue.message;
  }

  static media_router::IssueInfo::Action default_action(
      const media_router::IssueInfo& issue) {
    return issue.default_action;
  }

  static const std::vector<media_router::IssueInfo::Action>& secondary_actions(
      const media_router::IssueInfo& issue) {
    return issue.secondary_actions;
  }

  static int32_t help_page_id(const media_router::IssueInfo& issue) {
    return issue.help_page_id;
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
      case media_router::SinkIconType::MEETING:
        return media_router::mojom::SinkIconType::MEETING;
      case media_router::SinkIconType::HANGOUT:
        return media_router::mojom::SinkIconType::HANGOUT;
      case media_router::SinkIconType::EDUCATION:
        return media_router::mojom::SinkIconType::EDUCATION;
      case media_router::SinkIconType::WIRED_DISPLAY:
        return media_router::mojom::SinkIconType::WIRED_DISPLAY;
      case media_router::SinkIconType::GENERIC:
        return media_router::mojom::SinkIconType::GENERIC;
      case media_router::SinkIconType::TOTAL_COUNT:
        break;
    }
    NOTREACHED() << "Unknown sink icon type " << static_cast<int>(icon_type);
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
      case media_router::mojom::SinkIconType::MEETING:
        *output = media_router::SinkIconType::MEETING;
        return true;
      case media_router::mojom::SinkIconType::HANGOUT:
        *output = media_router::SinkIconType::HANGOUT;
        return true;
      case media_router::mojom::SinkIconType::EDUCATION:
        *output = media_router::SinkIconType::EDUCATION;
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

  static const base::Optional<std::string>& description(
      const media_router::MediaSinkInternal& sink_internal) {
    return sink_internal.sink().description();
  }

  static const base::Optional<std::string>& domain(
      const media_router::MediaSinkInternal& sink_internal) {
    return sink_internal.sink().domain();
  }

  static media_router::SinkIconType icon_type(
      const media_router::MediaSinkInternal& sink_internal) {
    return sink_internal.sink().icon_type();
  }

  static media_router::MediaRouteProviderId provider_id(
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
    NOTREACHED() << "Unknown controller type "
                 << static_cast<int>(controller_type);
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

  static base::Optional<std::string> media_source(
      const media_router::MediaRoute& route) {
    // TODO(imcheng): If we ever convert from C++ to Mojo outside of unit tests,
    // it would be better to make the |media_source_| field on MediaRoute a
    // base::Optional<MediaSource::Id> instead so it can be returned directly
    // here.
    return route.media_source().id().empty()
               ? base::Optional<std::string>()
               : base::make_optional(route.media_source().id());
  }

  static const std::string& media_sink_id(
      const media_router::MediaRoute& route) {
    return route.media_sink_id();
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

  static bool for_display(const media_router::MediaRoute& route) {
    return route.for_display();
  }

  static bool is_incognito(const media_router::MediaRoute& route) {
    return route.is_incognito();
  }

  static bool is_local_presentation(const media_router::MediaRoute& route) {
    return route.is_local_presentation();
  }
};

// RouteRequestResultCode

template <>
struct EnumTraits<media_router::mojom::RouteRequestResultCode,
                  media_router::RouteRequestResult::ResultCode> {
  static media_router::mojom::RouteRequestResultCode ToMojom(
      media_router::RouteRequestResult::ResultCode code) {
    switch (code) {
      case media_router::RouteRequestResult::UNKNOWN_ERROR:
        return media_router::mojom::RouteRequestResultCode::UNKNOWN_ERROR;
      case media_router::RouteRequestResult::OK:
        return media_router::mojom::RouteRequestResultCode::OK;
      case media_router::RouteRequestResult::TIMED_OUT:
        return media_router::mojom::RouteRequestResultCode::TIMED_OUT;
      case media_router::RouteRequestResult::ROUTE_NOT_FOUND:
        return media_router::mojom::RouteRequestResultCode::ROUTE_NOT_FOUND;
      case media_router::RouteRequestResult::SINK_NOT_FOUND:
        return media_router::mojom::RouteRequestResultCode::SINK_NOT_FOUND;
      case media_router::RouteRequestResult::INVALID_ORIGIN:
        return media_router::mojom::RouteRequestResultCode::INVALID_ORIGIN;
      case media_router::RouteRequestResult::INCOGNITO_MISMATCH:
        return media_router::mojom::RouteRequestResultCode::INCOGNITO_MISMATCH;
      case media_router::RouteRequestResult::NO_SUPPORTED_PROVIDER:
        return media_router::mojom::RouteRequestResultCode::
            NO_SUPPORTED_PROVIDER;
      case media_router::RouteRequestResult::CANCELLED:
        return media_router::mojom::RouteRequestResultCode::CANCELLED;
      case media_router::RouteRequestResult::ROUTE_ALREADY_EXISTS:
        return media_router::mojom::RouteRequestResultCode::
            ROUTE_ALREADY_EXISTS;
      case media_router::RouteRequestResult::DESKTOP_PICKER_FAILED:
        return media_router::mojom::RouteRequestResultCode::
            DESKTOP_PICKER_FAILED;
      default:
        NOTREACHED() << "Unknown RouteRequestResultCode "
                     << static_cast<int>(code);
        return media_router::mojom::RouteRequestResultCode::UNKNOWN_ERROR;
    }
  }

  static bool FromMojom(media_router::mojom::RouteRequestResultCode input,
                        media_router::RouteRequestResult::ResultCode* output) {
    switch (input) {
      case media_router::mojom::RouteRequestResultCode::UNKNOWN_ERROR:
        *output = media_router::RouteRequestResult::UNKNOWN_ERROR;
        return true;
      case media_router::mojom::RouteRequestResultCode::OK:
        *output = media_router::RouteRequestResult::OK;
        return true;
      case media_router::mojom::RouteRequestResultCode::TIMED_OUT:
        *output = media_router::RouteRequestResult::TIMED_OUT;
        return true;
      case media_router::mojom::RouteRequestResultCode::ROUTE_NOT_FOUND:
        *output = media_router::RouteRequestResult::ROUTE_NOT_FOUND;
        return true;
      case media_router::mojom::RouteRequestResultCode::SINK_NOT_FOUND:
        *output = media_router::RouteRequestResult::SINK_NOT_FOUND;
        return true;
      case media_router::mojom::RouteRequestResultCode::INVALID_ORIGIN:
        *output = media_router::RouteRequestResult::INVALID_ORIGIN;
        return true;
      case media_router::mojom::RouteRequestResultCode::INCOGNITO_MISMATCH:
        *output = media_router::RouteRequestResult::INCOGNITO_MISMATCH;
        return true;
      case media_router::mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER:
        *output = media_router::RouteRequestResult::NO_SUPPORTED_PROVIDER;
        return true;
      case media_router::mojom::RouteRequestResultCode::CANCELLED:
        *output = media_router::RouteRequestResult::CANCELLED;
        return true;
      case media_router::mojom::RouteRequestResultCode::ROUTE_ALREADY_EXISTS:
        *output = media_router::RouteRequestResult::ROUTE_ALREADY_EXISTS;
        return true;
      case media_router::mojom::RouteRequestResultCode::DESKTOP_PICKER_FAILED:
        *output = media_router::RouteRequestResult::DESKTOP_PICKER_FAILED;
        return true;
    }
    return false;
  }
};

// MediaRouteProvider

template <>
struct EnumTraits<media_router::mojom::MediaRouteProvider::Id,
                  media_router::MediaRouteProviderId> {
  static media_router::mojom::MediaRouteProvider::Id ToMojom(
      media_router::MediaRouteProviderId provider_id) {
    switch (provider_id) {
      case media_router::MediaRouteProviderId::EXTENSION:
        return media_router::mojom::MediaRouteProvider::Id::EXTENSION;
      case media_router::MediaRouteProviderId::WIRED_DISPLAY:
        return media_router::mojom::MediaRouteProvider::Id::WIRED_DISPLAY;
      case media_router::MediaRouteProviderId::CAST:
        return media_router::mojom::MediaRouteProvider::Id::CAST;
      case media_router::MediaRouteProviderId::DIAL:
        return media_router::mojom::MediaRouteProvider::Id::DIAL;
      case media_router::MediaRouteProviderId::UNKNOWN:
        break;
    }
    NOTREACHED() << "Invalid MediaRouteProvider::Id: "
                 << static_cast<int>(provider_id);
    return media_router::mojom::MediaRouteProvider::Id::EXTENSION;
  }

  static bool FromMojom(media_router::mojom::MediaRouteProvider::Id input,
                        media_router::MediaRouteProviderId* provider_id) {
    switch (input) {
      case media_router::mojom::MediaRouteProvider::Id::EXTENSION:
        *provider_id = media_router::MediaRouteProviderId::EXTENSION;
        return true;
      case media_router::mojom::MediaRouteProvider::Id::WIRED_DISPLAY:
        *provider_id = media_router::MediaRouteProviderId::WIRED_DISPLAY;
        return true;
      case media_router::mojom::MediaRouteProvider::Id::CAST:
        *provider_id = media_router::MediaRouteProviderId::CAST;
        return true;
      case media_router::mojom::MediaRouteProvider::Id::DIAL:
        *provider_id = media_router::MediaRouteProviderId::DIAL;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // CHROME_COMMON_MEDIA_ROUTER_MOJOM_MEDIA_ROUTER_MOJOM_TRAITS_H_
