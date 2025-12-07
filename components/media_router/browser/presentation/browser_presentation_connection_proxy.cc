// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/browser_presentation_connection_proxy.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/route_message_util.h"

namespace media_router {

BrowserPresentationConnectionProxy::BrowserPresentationConnectionProxy(
    MediaRouter* router,
    const MediaRoute::Id& route_id,
    mojo::PendingReceiver<blink::mojom::PresentationConnection>
        receiver_connection_receiver,
    mojo::PendingRemote<blink::mojom::PresentationConnection>
        controller_connection_remote)
    : PresentationConnectionMessageObserver(router, route_id),
      router_(router),
      route_id_(route_id),
      target_connection_remote_(std::move(controller_connection_remote)) {
  DCHECK(router);
  DCHECK(target_connection_remote_);

  // TODO(btolsch): |receiver_| and |target_connection_remote_| may need proper
  // mojo error handlers.  They probably need to be plumbed up to PSDImpl so the
  // PresentationFrame knows about the error.
  receiver_.Bind(std::move(receiver_connection_receiver));
  target_connection_remote_->DidChangeState(
      blink::mojom::PresentationConnectionState::CONNECTED);
}

BrowserPresentationConnectionProxy::~BrowserPresentationConnectionProxy() =
    default;

void BrowserPresentationConnectionProxy::OnMessage(
    blink::mojom::PresentationConnectionMessagePtr message) {
  if (message->is_data()) {
    router_->SendRouteBinaryMessage(
        route_id_,
        std::make_unique<std::vector<uint8_t>>(std::move(message->get_data())));
  } else {
    router_->SendRouteMessage(route_id_, message->get_message());
  }
}

void BrowserPresentationConnectionProxy::DidClose(
    blink::mojom::PresentationConnectionCloseReason reason) {
  // Closing PresentationConnection is handled by
  // PresentationService::CloseConnection or PresentationConnection implemented
  // by a Media Route Provider.
}

void BrowserPresentationConnectionProxy::OnMessagesReceived(
    std::vector<mojom::RouteMessagePtr> messages) {
  // TODO(imcheng): It would be slightly more efficient to send messages in
  // a single batch.
  for (auto& message : messages) {
    target_connection_remote_->OnMessage(
        message_util::PresentationConnectionFromRouteMessage(
            std::move(message)));
  }
}
}  // namespace media_router
