// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_CONNECTION_MESSAGE_OBSERVER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_CONNECTION_MESSAGE_OBSERVER_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_router.mojom.h"

namespace media_router {

class MediaRouter;

// Observes messages originating from the MediaSink connected to a MediaRoute.
// Messages are received from the MediaRouter via OnMessagesReceived().
// TODO(crbug.com/40177419): remove this observer class.
class PresentationConnectionMessageObserver : public base::CheckedObserver {
 public:
  // `route_id`: ID of MediaRoute to listen for messages.
  PresentationConnectionMessageObserver(MediaRouter* router,
                                        const MediaRoute::Id& route_id);

  PresentationConnectionMessageObserver(
      const PresentationConnectionMessageObserver&) = delete;
  PresentationConnectionMessageObserver(
      PresentationConnectionMessageObserver&&) = delete;
  PresentationConnectionMessageObserver& operator=(
      PresentationConnectionMessageObserver&&) = delete;
  PresentationConnectionMessageObserver& operator=(
      const PresentationConnectionMessageObserver&) = delete;

  ~PresentationConnectionMessageObserver() override;

  // Invoked by `router_` whenever messages are received from the route sink.
  // `messages` is guaranteed to be non-empty.
  virtual void OnMessagesReceived(
      std::vector<mojom::RouteMessagePtr> messages) = 0;

  const MediaRoute::Id& route_id() const { return route_id_; }

 private:
  const raw_ptr<MediaRouter> router_;
  const MediaRoute::Id route_id_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_CONNECTION_MESSAGE_OBSERVER_H_
