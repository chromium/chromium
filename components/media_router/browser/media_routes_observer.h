// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTES_OBSERVER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTES_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "components/media_router/common/media_route.h"

namespace media_router {

class MediaRouter;

// Base class for observing when the set of MediaRoutes and their associated
// MediaSinks have been updated.  When an object is instantiated with a
// |source_id|, the observer expects that |routes| reported by
// |OnRoutesUpdated| that match the route IDs contained in the
// |joinable_route_ids| can be connected joined by the source.  If no
// |source_id| is supplied, then the idea of joinable routes no longer applies.
class MediaRoutesObserver {
 public:
  explicit MediaRoutesObserver(MediaRouter* router)
      : MediaRoutesObserver(router, MediaSource::Id()) {}
  MediaRoutesObserver(MediaRouter* router, const MediaSource::Id& source_id);
  virtual ~MediaRoutesObserver();

  // Invoked when the list of routes and their associated sinks have been
  // updated with the context of the |source_id|.  This will return a list of
  // |routes| and a list of |joinable_route_ids|.  A route is joinable only if
  // it is joinable in the context of the |source_id|.
  // Implementations may not perform operations that modify the Media Router's
  // observer list. In particular, invoking this observer's destructor within
  // OnRoutesUpdated will result in undefined behavior.
  virtual void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids) {}

  MediaRouter* router() const { return router_; }
  const MediaSource::Id source_id() const { return source_id_; }

 private:
  MediaRouter* const router_;
  const MediaSource::Id source_id_;

  DISALLOW_COPY_AND_ASSIGN(MediaRoutesObserver);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTES_OBSERVER_H_
