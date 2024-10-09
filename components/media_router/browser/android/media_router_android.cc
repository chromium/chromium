// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/android/media_router_android.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/media_sinks_observer.h"
#include "components/media_router/browser/presentation_connection_message_observer.h"
#include "components/media_router/browser/route_message_util.h"
#include "components/media_router/common/route_request_result.h"
#include "url/gurl.h"

namespace media_router {

MediaRouterAndroid::PresentationConnectionProxy::PresentationConnectionProxy(
    MediaRouterAndroid* media_router_android,
    const MediaRoute::Id& route_id)
    : media_router_android_(media_router_android), route_id_(route_id) {}

MediaRouterAndroid::PresentationConnectionProxy::
    ~PresentationConnectionProxy() = default;

mojom::RoutePresentationConnectionPtr
MediaRouterAndroid::PresentationConnectionProxy::Init() {
  auto receiver = peer_.BindNewPipeAndPassReceiver();
  peer_.set_disconnect_handler(
      base::BindOnce(&MediaRouterAndroid::OnPresentationConnectionError,
                     base::Unretained(media_router_android_), route_id_));
  peer_->DidChangeState(blink::mojom::PresentationConnectionState::CONNECTED);
  return mojom::RoutePresentationConnection::New(Bind(), std::move(receiver));
}

void MediaRouterAndroid::PresentationConnectionProxy::OnMessage(
    blink::mojom::PresentationConnectionMessagePtr message) {
  if (message->is_message())
    media_router_android_->SendRouteMessage(route_id_, message->get_message());
}

void MediaRouterAndroid::PresentationConnectionProxy::Terminate() {
  DCHECK(peer_);
  peer_->DidChangeState(blink::mojom::PresentationConnectionState::TERMINATED);
}

void MediaRouterAndroid::PresentationConnectionProxy::DidClose(
    blink::mojom::PresentationConnectionCloseReason reason) {
  auto& route_connections =
      media_router_android_->presentation_connections_[route_id_];
  DCHECK(!route_connections.empty());
  std::erase_if(route_connections, [this](const auto& connection) {
    return connection.get() == this;
  });
}

mojo::PendingRemote<blink::mojom::PresentationConnection>
MediaRouterAndroid::PresentationConnectionProxy::Bind() {
  mojo::PendingRemote<blink::mojom::PresentationConnection> connection_remote;
  receiver_.Bind(connection_remote.InitWithNewPipeAndPassReceiver());
  receiver_.set_disconnect_handler(
      base::BindOnce(&MediaRouterAndroid::OnPresentationConnectionError,
                     base::Unretained(media_router_android_), route_id_));
  return connection_remote;
}

void MediaRouterAndroid::PresentationConnectionProxy::SendMessage(
    const std::string& message) {
  DCHECK(peer_);
  peer_->OnMessage(
      blink::mojom::PresentationConnectionMessage::NewMessage(message));
}

MediaRouterAndroid::MediaRouteRequest::MediaRouteRequest(
    const MediaSource& source,
    const std::string& presentation_id,
    MediaRouteResponseCallback callback)
    : media_source(source),
      presentation_id(presentation_id),
      callback(std::move(callback)) {}

MediaRouterAndroid::MediaRouteRequest::~MediaRouteRequest() {}

MediaRouterAndroid::MediaRouterAndroid()
    : bridge_(new MediaRouterAndroidBridge(this)) {}

MediaRouterAndroid::~MediaRouterAndroid() = default;

const MediaRoute* MediaRouterAndroid::FindRouteBySource(
    const MediaSource::Id& source_id) const {
  for (const auto& route : active_routes_) {
    if (route.media_source().id() == source_id)
      return &route;
  }
  return nullptr;
}

void MediaRouterAndroid::Initialize() {}

void MediaRouterAndroid::CreateRoute(const MediaSource::Id& source_id,
                                     const MediaSink::Id& sink_id,
                                     const url::Origin& origin,
                                     content::WebContents* web_contents,
                                     MediaRouteResponseCallback callback,
                                     base::TimeDelta timeout) {
  DCHECK(callback);
  // TODO(avayvod): Implement timeouts (crbug.com/583036).
  std::string presentation_id = MediaRouterBase::CreatePresentationId();

  int route_request_id =
      route_requests_.Add(std::make_unique<MediaRouteRequest>(
          MediaSource(source_id), presentation_id, std::move(callback)));
  bridge_->CreateRoute(source_id, sink_id, presentation_id, origin,
                       web_contents, route_request_id);
}

void MediaRouterAndroid::JoinRoute(const MediaSource::Id& source_id,
                                   const std::string& presentation_id,
                                   const url::Origin& origin,
                                   content::WebContents* web_contents,
                                   MediaRouteResponseCallback callback,
                                   base::TimeDelta timeout) {
  DCHECK(callback);
  // TODO(avayvod): Implement timeouts (crbug.com/583036).
  DVLOG(2) << "JoinRoute: " << source_id << ", " << presentation_id << ", "
           << origin.GetURL().spec();

  int request_id = route_requests_.Add(std::make_unique<MediaRouteRequest>(
      MediaSource(source_id), presentation_id, std::move(callback)));
  bridge_->JoinRoute(source_id, presentation_id, origin, web_contents,
                     request_id);
}

void MediaRouterAndroid::TerminateRoute(const MediaRoute::Id& route_id) {
  bridge_->TerminateRoute(route_id);
}

void MediaRouterAndroid::SendRouteMessage(const MediaRoute::Id& route_id,
                                          const std::string& message) {
  bridge_->SendRouteMessage(route_id, message);
}

void MediaRouterAndroid::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data) {
  // Binary messaging is not supported on Android.
}

void MediaRouterAndroid::OnUserGesture() {}

std::vector<MediaRoute> MediaRouterAndroid::GetCurrentRoutes() const {
  return active_routes_;
}

void MediaRouterAndroid::DetachRoute(MediaRoute::Id route_id) {
  bridge_->DetachRoute(route_id);
  RemoveRoute(route_id);
  NotifyPresentationConnectionClose(
      route_id, blink::mojom::PresentationConnectionCloseReason::CLOSED,
      "Route closed normally");
}

bool MediaRouterAndroid::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  const std::string& source_id = observer->source()->id();
  auto& observer_list = sinks_observers_[source_id];
  if (!observer_list) {
    observer_list = std::make_unique<MediaSinksObserverList>();
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  observer_list->AddObserver(observer);
  return bridge_->StartObservingMediaSinks(source_id);
}

void MediaRouterAndroid::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  const std::string& source_id = observer->source()->id();
  auto it = sinks_observers_.find(source_id);
  if (it == sinks_observers_.end() || !it->second->HasObserver(observer))
    return;

  // If we are removing the final observer for the source, then stop
  // observing sinks for it.
  it->second->RemoveObserver(observer);
  if (it->second->empty()) {
    sinks_observers_.erase(source_id);
    bridge_->StopObservingMediaSinks(source_id);
  }
}

void MediaRouterAndroid::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  DVLOG(2) << "Added MediaRoutesObserver: " << observer;
  routes_observers_.AddObserver(observer);
}

void MediaRouterAndroid::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  if (!routes_observers_.HasObserver(observer))
    return;
  routes_observers_.RemoveObserver(observer);
}

void MediaRouterAndroid::RegisterPresentationConnectionMessageObserver(
    PresentationConnectionMessageObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void MediaRouterAndroid::UnregisterPresentationConnectionMessageObserver(
    PresentationConnectionMessageObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void MediaRouterAndroid::OnSinksReceived(const std::string& source_urn,
                                         const std::vector<MediaSink>& sinks) {
  auto it = sinks_observers_.find(source_urn);
  if (it != sinks_observers_.end()) {
    // TODO(imcheng): Pass origins to OnSinksUpdated (crbug.com/594858).
    for (auto& observer : *it->second)
      observer.OnSinksUpdated(sinks, std::vector<url::Origin>());
  }
}

void MediaRouterAndroid::OnRouteCreated(const MediaRoute::Id& route_id,
                                        const MediaSink::Id& sink_id,
                                        int route_request_id,
                                        bool is_local) {
  MediaRouteRequest* request = route_requests_.Lookup(route_request_id);
  if (!request)
    return;

  MediaRoute route(route_id, request->media_source, sink_id, std::string(),
                   is_local);

  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromSuccess(route, request->presentation_id);
  auto& presentation_connections = presentation_connections_[route_id];
  presentation_connections.push_back(
      std::make_unique<PresentationConnectionProxy>(this, route_id));
  auto& presentation_connection = *presentation_connections.back();
  std::move(request->callback).Run(presentation_connection.Init(), *result);

  route_requests_.Remove(route_request_id);

  active_routes_.push_back(route);
  for (auto& observer : routes_observers_)
    observer.OnRoutesUpdated(active_routes_);
  if (is_local) {
    MediaRouterMetrics::RecordCreateRouteResultCode(
        result->result_code(), mojom::MediaRouteProviderId::ANDROID_CAF);
  } else {
    MediaRouterMetrics::RecordJoinRouteResultCode(
        result->result_code(), mojom::MediaRouteProviderId::ANDROID_CAF);
  }
}

void MediaRouterAndroid::OnRouteMediaSourceUpdated(
    const MediaRoute::Id& route_id,
    const MediaSource::Id& source_id) {
  for (auto& route : active_routes_) {
    if (route.media_route_id() == route_id) {
      route.set_media_source(MediaSource(source_id));
      break;
    }
  }

  for (auto& observer : routes_observers_) {
    observer.OnRoutesUpdated(active_routes_);
  }
}

void MediaRouterAndroid::OnCreateRouteRequestError(
    const std::string& error_text,
    int route_request_id) {
  OnRouteRequestError(
      error_text, route_request_id,
      base::BindOnce(&MediaRouterMetrics::RecordCreateRouteResultCode));
}

void MediaRouterAndroid::OnJoinRouteRequestError(const std::string& error_text,
                                                 int route_request_id) {
  OnRouteRequestError(
      error_text, route_request_id,
      base::BindOnce(&MediaRouterMetrics::RecordJoinRouteResultCode));
}

void MediaRouterAndroid::OnRouteTerminated(const MediaRoute::Id& route_id) {
  auto entry = presentation_connections_.find(route_id);
  if (entry != presentation_connections_.end()) {
    // Note: Route-ID-to-presentation-ID mapping is done by route providers.
    // Although the messages API (being deprecated) is based on route IDs,
    // providers may use the same route for each presentation connection.  This
    // would result in broadcasting provider messages to all presentation
    // connections.  So although this loop may seem strange in the context of
    // the Presentation API, it can't be avoided at the moment.
    for (auto& connection : entry->second) {
      connection->Terminate();
    }
  }
  MediaRouterMetrics::RecordMediaRouteProviderTerminateRoute(
      mojom::RouteRequestResultCode::OK,
      mojom::MediaRouteProviderId::ANDROID_CAF);
  RemoveRoute(route_id);
}

void MediaRouterAndroid::OnRouteClosed(
    const MediaRoute::Id& route_id,
    const std::optional<std::string>& error) {
  RemoveRoute(route_id);
  // TODO(crbug.com/40593074): When the sending context is destroyed, tell MRP
  // to clean up the connection.
  if (error.has_value()) {
    NotifyPresentationConnectionClose(
        route_id,
        blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR,
        error.value());
  } else {
    NotifyPresentationConnectionClose(
        route_id, blink::mojom::PresentationConnectionCloseReason::CLOSED,
        "Remove route");
  }
}

void MediaRouterAndroid::OnMessage(const MediaRoute::Id& route_id,
                                   const std::string& message) {
  auto entry = presentation_connections_.find(route_id);
  if (entry != presentation_connections_.end()) {
    // Note: Route-ID-to-presentation-ID mapping is done by route providers.
    // Although the messages API (being deprecated) is based on route IDs,
    // providers may use the same route for each presentation connection.  This
    // would result in broadcasting provider messages to all presentation
    // connections.  So although this loop may seem strange in the context of
    // the Presentation API, it can't be avoided at the moment.
    DCHECK_EQ(1u, entry->second.size());
    for (auto& connection : entry->second) {
      connection->SendMessage(message);
    }
  }
}

void MediaRouterAndroid::RemoveRoute(const MediaRoute::Id& route_id) {
  presentation_connections_.erase(route_id);
  for (auto it = active_routes_.begin(); it != active_routes_.end(); ++it)
    if (it->media_route_id() == route_id) {
      active_routes_.erase(it);
      break;
    }

  for (auto& observer : routes_observers_)
    observer.OnRoutesUpdated(active_routes_);
}

std::unique_ptr<media::FlingingController>
MediaRouterAndroid::GetFlingingController(const MediaRoute::Id& route_id) {
  return bridge_->GetFlingingController(route_id);
}

void MediaRouterAndroid::OnPresentationConnectionError(
    const std::string& route_id) {
  presentation_connections_.erase(route_id);
}

void MediaRouterAndroid::OnRouteRequestError(
    const std::string& error_text,
    int route_request_id,
    base::OnceCallback<void(mojom::RouteRequestResultCode,
                            std::optional<mojom::MediaRouteProviderId>)>
        callback) {
  MediaRouteRequest* request = route_requests_.Lookup(route_request_id);
  if (!request)
    return;

  // TODO: Provide a more specific result code.
  std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
      error_text, mojom::RouteRequestResultCode::UNKNOWN_ERROR);
  std::move(request->callback).Run(nullptr, *result);

  route_requests_.Remove(route_request_id);
  std::move(callback).Run(result->result_code(),
                          mojom::MediaRouteProviderId::ANDROID_CAF);
}

}  // namespace media_router
