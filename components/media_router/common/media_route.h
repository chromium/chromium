// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_H_

#include <iosfwd>
#include <string>

#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"

namespace media_router {

// TODO(imcheng): Use the Mojo enum directly once we Mojo-ified
// MediaRouterAndroid.
enum class RouteControllerType { kNone, kGeneric, kMirroring };

// MediaRoute objects contain the status and metadata of a media route, used by
// Chrome to transmit and manage media on another device.
//
// TODO(mfoltz): Convert to a simple struct and remove uncommon parameters from
// the ctor.
class MediaRoute {
 public:
  using Id = std::string;

  static MediaRoute::Id GetMediaRouteId(const std::string& presentation_id,
                                        const MediaSink::Id& sink_id,
                                        const MediaSource& source);
  static std::string GetPresentationIdFromMediaRouteId(
      const MediaRoute::Id route_id);
  static std::string GetSinkIdFromMediaRouteId(const MediaRoute::Id route_id);
  static std::string GetMediaSourceIdFromMediaRouteId(
      const MediaRoute::Id route_id);

  // All hand-written code MUST use this constructor.
  //
  // |media_route_id|: ID of the route.
  // |media_source|: Description of source of the route.
  // |media_sink|: The sink that is receiving the media.
  // |description|: Human readable description of the casting activity.
  // |is_local|: true if the route was created from this browser.
  MediaRoute(const MediaRoute::Id& media_route_id,
             const MediaSource& media_source,
             const MediaSink::Id& media_sink_id,
             const std::string& description,
             bool is_local);

  // DO NOT USE.  No-arg constructor only for use by mojo.
  MediaRoute();
  MediaRoute(const MediaRoute&);
  MediaRoute& operator=(const MediaRoute&);
  MediaRoute(MediaRoute&&);
  MediaRoute& operator=(MediaRoute&&);
  ~MediaRoute();

  void set_media_route_id(const MediaRoute::Id& media_route_id) {
    media_route_id_ = media_route_id;
  }
  const MediaRoute::Id& media_route_id() const { return media_route_id_; }

  void set_presentation_id(const std::string& presentation_id) {
    presentation_id_ = presentation_id;
  }
  const std::string& presentation_id() const { return presentation_id_; }

  void set_media_source(const MediaSource& media_source) {
    media_source_ = media_source;
  }
  const MediaSource& media_source() const { return media_source_; }

  void set_media_sink_id(const MediaSink::Id& media_sink_id) {
    media_sink_id_ = media_sink_id;
  }
  const MediaSink::Id& media_sink_id() const { return media_sink_id_; }

  void set_media_sink_name(const std::string& media_sink_name) {
    media_sink_name_ = media_sink_name;
  }
  const std::string& media_sink_name() const { return media_sink_name_; }

  void set_description(const std::string& description) {
    description_ = description;
  }

  // TODO(kmarshall): Do we need to pass locale for bidi rendering?
  const std::string& description() const { return description_; }

  void set_local(bool is_local) { is_local_ = is_local; }
  bool is_local() const { return is_local_; }

  void set_controller_type(RouteControllerType controller_type) {
    controller_type_ = controller_type;
  }
  RouteControllerType controller_type() const { return controller_type_; }

  void set_local_presentation(bool is_local_presentation) {
    is_local_presentation_ = is_local_presentation;
  }
  bool is_local_presentation() const { return is_local_presentation_; }

  void set_is_connecting(bool is_connecting) { is_connecting_ = is_connecting; }
  bool is_connecting() const { return is_connecting_; }

  bool IsLocalMirroringRoute() const;

  bool operator==(const MediaRoute& other) const;

 private:
  friend std::ostream& operator<<(std::ostream& stream,
                                  const MediaRoute& route);

  // The media route identifier.
  MediaRoute::Id media_route_id_;

  // The ID of the presentation that this route is associated with.
  std::string presentation_id_;

  // The media source being routed.
  MediaSource media_source_;

  // The ID of sink being routed to.
  MediaSink::Id media_sink_id_;

  // Human readable name of the sink.
  std::string media_sink_name_;

  // Human readable description of the casting activity.  Examples:
  // "Mirroring tab (www.example.com)", "Casting media", "Casting YouTube"
  std::string description_;

  // |true| if the route is created locally (versus discovered by a media route
  // provider.)
  bool is_local_ = false;

  // The type of MediaRouteController supported by this route.
  RouteControllerType controller_type_ = RouteControllerType::kNone;

  // |true| if the presentation associated with this route is a local
  // presentation.
  // TODO(crbug.com/40219575): Remove |is_local_presentation_|.
  bool is_local_presentation_ = false;

  // |true| if the route is created by the MRP but is waiting for receivers'
  // response.
  bool is_connecting_ = false;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_ROUTE_H_
