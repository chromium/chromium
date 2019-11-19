// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_H_
#define CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_H_

#include <iosfwd>
#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"

namespace media_router {

// TODO(imcheng): Use the Mojo enum directly once we Mojo-ified
// MediaRouterAndroid.
enum class RouteControllerType { kNone, kGeneric, kMirroring };

// MediaRoute objects contain the status and metadata of a routing
// operation. The fields are immutable and reflect the route status
// only at the time of object creation. Updated route statuses must
// be retrieved as new MediaRoute objects from the Media Router.
//
// TODO(mfoltz): Convert to a simple struct and remove uncommon parameters from
// the ctor.
class MediaRoute {
 public:
  using Id = std::string;

  static MediaRoute::Id GetMediaRouteId(const std::string& presentation_id,
                                        const MediaSink::Id& sink_id,
                                        const MediaSource& source);

  // |media_route_id|: ID of the route.
  // |media_source|: Description of source of the route.
  // |media_sink|: The sink that is receiving the media.
  // |description|: Human readable description of the casting activity.
  // |is_local|: true if the route was created from this browser.
  //     provider. empty otherwise.
  // |for_display|: Set to true if this route should be displayed for
  //     |media_sink_id| in UI.
  MediaRoute(const MediaRoute::Id& media_route_id,
             const MediaSource& media_source,
             const MediaSink::Id& media_sink_id,
             const std::string& description,
             bool is_local,
             bool for_display);
  MediaRoute(const MediaRoute& other);
  MediaRoute();

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

  void set_for_display(bool for_display) { for_display_ = for_display; }
  bool for_display() const { return for_display_; }

  void set_incognito(bool is_incognito) { is_incognito_ = is_incognito; }
  bool is_incognito() const { return is_incognito_; }

  void set_local_presentation(bool is_local_presentation) {
    is_local_presentation_ = is_local_presentation;
  }
  bool is_local_presentation() const { return is_local_presentation_; }

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

  // Human readable description of the casting activity.  Examples:
  // "Mirroring tab (www.example.com)", "Casting media", "Casting YouTube"
  std::string description_;

  // |true| if the route is created locally (versus discovered by a media route
  // provider.)
  bool is_local_ = false;

  // The type of MediaRouteController supported by this route.
  RouteControllerType controller_type_ = RouteControllerType::kNone;

  // |true| if the route can be displayed in the UI.
  bool for_display_ = false;

  // |true| if the route was created by an incognito profile.
  bool is_incognito_ = false;

  // |true| if the presentation associated with this route is a local
  // presentation.
  bool is_local_presentation_ = false;
};

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_H_
