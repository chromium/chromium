// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_base.h"

#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

using blink::mojom::PresentationConnectionState;

namespace media_router {

// A MediaRoutesObserver that maintains state about the current set of media
// routes.
//
// TODO(crbug.com/1297324): This observer is used to implement
// HasJoinableRoute() and GetRoute(), which are used internally by
// MediaRouterMojoImpl; it would be simpler to move route tracking to a separate
// object owned by MRMI, and then MediaRouterBase could likely be deleted
// entirely.
class MediaRouterBase::InternalMediaRoutesObserver
    : public MediaRoutesObserver {
 public:
  explicit InternalMediaRoutesObserver(MediaRouter* router)
      : MediaRoutesObserver(router) {}

  InternalMediaRoutesObserver(const InternalMediaRoutesObserver&) = delete;
  InternalMediaRoutesObserver& operator=(const InternalMediaRoutesObserver&) =
      delete;

  ~InternalMediaRoutesObserver() override {}

  // MediaRoutesObserver
  void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override {
    current_routes_ = routes;
  }

  const std::vector<MediaRoute>& current_routes() const {
    return current_routes_;
  }

 private:
  std::vector<MediaRoute> current_routes_;
};

MediaRouterBase::~MediaRouterBase() {
  CHECK(!internal_routes_observer_);
}

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

IssueManager* MediaRouterBase::GetIssueManager() {
  return &issue_manager_;
}

std::vector<MediaRoute> MediaRouterBase::GetCurrentRoutes() const {
  return internal_routes_observer_->current_routes();
}

std::unique_ptr<media::FlingingController>
MediaRouterBase::GetFlingingController(const MediaRoute::Id& route_id) {
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
void MediaRouterBase::GetMediaController(
    const MediaRoute::Id& route_id,
    mojo::PendingReceiver<mojom::MediaController> controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {}

base::Value MediaRouterBase::GetLogs() const {
  return base::Value();
}
#endif  // !BUILDFLAG(IS_ANDROID)

MediaRouterBase::MediaRouterBase() : initialized_(false) {}

// static
std::string MediaRouterBase::CreatePresentationId() {
  return "mr_" + base::GenerateGUID();
}

void MediaRouterBase::NotifyPresentationConnectionStateChange(
    const MediaRoute::Id& route_id,
    PresentationConnectionState state) {
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
  auto it = presentation_connection_state_callbacks_.find(route_id);
  if (it == presentation_connection_state_callbacks_.end())
    return;

  content::PresentationConnectionStateChangeInfo info(
      PresentationConnectionState::CLOSED);
  info.close_reason = reason;
  info.message = message;
  it->second->Notify(info);
}

bool MediaRouterBase::HasJoinableRoute() const {
  return !(internal_routes_observer_->current_routes().empty());
}

const MediaRoute* MediaRouterBase::GetRoute(
    const MediaRoute::Id& route_id) const {
  const auto& routes = internal_routes_observer_->current_routes();
  auto it = std::find_if(routes.begin(), routes.end(),
                         [&route_id](const MediaRoute& route) {
                           return route.media_route_id() == route_id;
                         });
  return it == routes.end() ? nullptr : &*it;
}

void MediaRouterBase::Initialize() {
  DCHECK(!initialized_);
  // The observer calls virtual methods on MediaRouter; it must be created
  // outside of the ctor
  internal_routes_observer_ =
      std::make_unique<InternalMediaRoutesObserver>(this);
  initialized_ = true;
}

void MediaRouterBase::OnPresentationConnectionStateCallbackRemoved(
    const MediaRoute::Id& route_id) {
  auto it = presentation_connection_state_callbacks_.find(route_id);
  if (it != presentation_connection_state_callbacks_.end() &&
      it->second->empty()) {
    presentation_connection_state_callbacks_.erase(route_id);
  }
}

void MediaRouterBase::Shutdown() {
  // The observer calls virtual methods on MediaRouter; it must be destroyed
  // outside of the dtor
  internal_routes_observer_.reset();
}

base::Value MediaRouterBase::GetState() const {
  NOTREACHED() << "Should not invoke MediaRouterBase::GetState()";
  return base::Value(base::Value::Type::DICTIONARY);
}

void MediaRouterBase::GetProviderState(
    mojom::MediaRouteProviderId provider_id,
    mojom::MediaRouteProvider::GetStateCallback callback) const {
  NOTREACHED() << "Should not invoke MediaRouterBase::GetProviderState()";
  std::move(callback).Run(mojom::ProviderStatePtr());
}

}  // namespace media_router
