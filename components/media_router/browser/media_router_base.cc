// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_base.h"

#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"

using blink::mojom::PresentationConnectionState;

namespace media_router {

// A MediaRoutesObserver that maintains state about the current set of media
// routes.
class MediaRouterBase::InternalMediaRoutesObserver
    : public MediaRoutesObserver {
 public:
  explicit InternalMediaRoutesObserver(MediaRouter* router)
      : MediaRoutesObserver(router), has_route(false) {}
  ~InternalMediaRoutesObserver() override {}

  // MediaRoutesObserver
  void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids) override {
    current_routes = routes;
    off_the_record_route_ids.clear();
    // TODO(crbug.com/611486): Have the MRPM pass a list of joinable route ids
    // via |joinable_route_ids|, and check here if it is non-empty.
    has_route = !routes.empty();
    for (const auto& route : routes) {
      if (route.is_off_the_record())
        off_the_record_route_ids.push_back(route.media_route_id());
    }
  }

  bool has_route;
  std::vector<MediaRoute> current_routes;
  std::vector<MediaRoute::Id> off_the_record_route_ids;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalMediaRoutesObserver);
};

MediaRouterBase::~MediaRouterBase() {
  CHECK(!internal_routes_observer_);
}

std::unique_ptr<PresentationConnectionStateSubscription>
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

void MediaRouterBase::OnIncognitoProfileShutdown() {
  for (const auto& route_id :
       internal_routes_observer_->off_the_record_route_ids)
    TerminateRoute(route_id);
}

IssueManager* MediaRouterBase::GetIssueManager() {
  return &issue_manager_;
}

std::vector<MediaRoute> MediaRouterBase::GetCurrentRoutes() const {
  return internal_routes_observer_->current_routes;
}

std::unique_ptr<media::FlingingController>
MediaRouterBase::GetFlingingController(const MediaRoute::Id& route_id) {
  return nullptr;
}

#if !defined(OS_ANDROID)
void MediaRouterBase::GetMediaController(
    const MediaRoute::Id& route_id,
    mojo::PendingReceiver<mojom::MediaController> controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {}

base::Value MediaRouterBase::GetLogs() const {
  return base::Value();
}
#endif  // !defined(OS_ANDROID)

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
  return internal_routes_observer_->has_route;
}

const MediaRoute* MediaRouterBase::GetRoute(
    const MediaRoute::Id& route_id) const {
  const auto& routes = internal_routes_observer_->current_routes;
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
  internal_routes_observer_.reset(new InternalMediaRoutesObserver(this));
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
    MediaRouteProviderId provider_id,
    mojom::MediaRouteProvider::GetStateCallback callback) const {
  NOTREACHED() << "Should not invoke MediaRouterBase::GetProviderState()";
  std::move(callback).Run(mojom::ProviderStatePtr());
}

}  // namespace media_router
