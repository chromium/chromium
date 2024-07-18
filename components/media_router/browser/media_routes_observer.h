// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTES_OBSERVER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTES_OBSERVER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "components/media_router/common/media_route.h"

namespace media_router {

class MediaRouter;

// Base class for observing when the set of MediaRoutes and their associated
// MediaSinks have been updated.  When an object is instantiated with a
// |source_id|, the observer expects that |routes| reported by
// |OnRoutesUpdated| that match the route IDs contained in the
// |joinable_route_ids| can be connected joined by the source.  If no
// |source_id| is supplied, then the idea of joinable routes no longer applies.
class MediaRoutesObserver : public base::CheckedObserver {
 public:
  explicit MediaRoutesObserver(MediaRouter* router);

  MediaRoutesObserver(const MediaRoutesObserver&) = delete;
  MediaRoutesObserver& operator=(const MediaRoutesObserver&) = delete;

  // NOTE: must be destroyed on the Browser UI thread to avoid threading issues
  // with access.
  ~MediaRoutesObserver() override;

  // Invoked when the list of routes and their associated sinks have been
  // updated.
  //
  // Implementations may not perform operations that modify the Media Router's
  // observer list. In particular, invoking this observer's destructor within
  // OnRoutesUpdated will result in undefined behavior.
  virtual void OnRoutesUpdated(const std::vector<MediaRoute>& routes) {}

  MediaRouter* router() const { return router_; }

 private:
  const raw_ptr<MediaRouter> router_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTES_OBSERVER_H_
