// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_base.h"

#include <string>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/uuid.h"
#include "content/public/browser/browser_thread.h"

using blink::mojom::PresentationConnectionState;

namespace media_router {

MediaRouterBase::~MediaRouterBase() = default;

base::CallbackListSubscription
MediaRouterBase::AddPresentationConnectionStateChangedCallback(
    const MediaRoute::Id& route_id,
    const content::PresentationConnectionStateChangedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto& callbacks = presentation_connection_state_callbacks_[route_id];
  if (!callbacks) {
    callbacks = std::make_unique<PresentationConnectionStateChangedCallbacks>();
    callbacks->set_removal_callback(base::BindRepeating(
        &MediaRouterBase::OnPresentationConnectionStateCallbackRemoved,
        base::Unretained(this), route_id));
  }

  return callbacks->Add(callback);
}

MediaRouterBase::MediaRouterBase() = default;

// static
std::string MediaRouterBase::CreatePresentationId() {
  return "mr_" + base::Uuid::GenerateRandomV4().AsLowercaseString();
}

void MediaRouterBase::NotifyPresentationConnectionStateChange(
    const MediaRoute::Id& route_id,
    PresentationConnectionState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We should call NotifyPresentationConnectionClose() for the CLOSED state.
  DCHECK_NE(state, PresentationConnectionState::CLOSED);

  auto it = presentation_connection_state_callbacks_.find(route_id);
  if (it == presentation_connection_state_callbacks_.end())
    return;

  it->second->Notify(content::PresentationConnectionStateChangeInfo(state));
}

void MediaRouterBase::NotifyPresentationConnectionClose(
    const MediaRoute::Id& route_id,
    blink::mojom::PresentationConnectionCloseReason reason,
    const std::string& message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = presentation_connection_state_callbacks_.find(route_id);
  if (it == presentation_connection_state_callbacks_.end())
    return;

  content::PresentationConnectionStateChangeInfo info(
      PresentationConnectionState::CLOSED);
  info.close_reason = reason;
  info.message = message;
  it->second->Notify(info);
}

void MediaRouterBase::OnPresentationConnectionStateCallbackRemoved(
    const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = presentation_connection_state_callbacks_.find(route_id);
  if (it != presentation_connection_state_callbacks_.end() &&
      it->second->empty()) {
    presentation_connection_state_callbacks_.erase(route_id);
  }
}

}  // namespace media_router
