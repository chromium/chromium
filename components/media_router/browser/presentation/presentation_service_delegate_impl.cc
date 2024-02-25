// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/small_map.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/browser/presentation/browser_presentation_connection_proxy.h"
#include "components/media_router/browser/presentation/local_presentation_manager.h"
#include "components/media_router/browser/presentation/local_presentation_manager_factory.h"
#include "components/media_router/browser/presentation/presentation_media_sinks_observer.h"
#include "components/media_router/browser/presentation_connection_message_observer.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/presentation_observer.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "url/gurl.h"

using blink::mojom::PresentationConnection;
using blink::mojom::PresentationError;
using blink::mojom::PresentationErrorType;
using blink::mojom::PresentationInfo;
using blink::mojom::ScreenAvailability;
using content::RenderFrameHost;

namespace media_router {

namespace {

using DelegateObserver = content::PresentationServiceDelegate::Observer;

// Gets the last committed URL for the render frame specified by
// |render_frame_host_id|.
url::Origin GetLastCommittedURLForFrame(
    content::GlobalRenderFrameHostId render_frame_host_id) {
  RenderFrameHost* render_frame_host = RenderFrameHost::FromID(
      render_frame_host_id.child_id, render_frame_host_id.frame_routing_id);
  DCHECK(render_frame_host);
  return render_frame_host->GetLastCommittedOrigin();
}

bool ArePresentationRequestsEqual(
    const content::PresentationRequest& request1,
    const content::PresentationRequest& request2) {
  return request1.render_frame_host_id == request2.render_frame_host_id &&
         request1.presentation_urls == request2.presentation_urls &&
         ((request1.frame_origin.opaque() && request2.frame_origin.opaque()) ||
          (request1.frame_origin == request2.frame_origin));
}

}  // namespace

// PresentationFrame interfaces with MediaRouter to maintain the current state
// of Presentation API within a single render frame, such as the set of
// PresentationAvailability listeners and PresentationConnections.
// Instances are lazily created when certain Presentation API is invoked on a
// frame, and are owned by PresentationServiceDelegateImpl.
// Instances are destroyed when the corresponding frame navigates, or when it
// is destroyed.
class PresentationFrame {
 public:
  PresentationFrame(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      content::WebContents* web_contents,
      MediaRouter* router);
  ~PresentationFrame();

  // Mirror corresponding APIs in PresentationServiceDelegateImpl.
  bool SetScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  void RemoveScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  bool HasScreenAvailabilityListenerForTest(
      const MediaSource::Id& source_id) const;
  void ListenForConnectionStateChange(
      const PresentationInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb);

  void Reset();

  MediaRoute::Id GetRouteId(const std::string& presentation_id) const;

  void AddPresentation(const PresentationInfo& presentation_info,
                       const MediaRoute& route);
  void ConnectToPresentation(
      const PresentationInfo& presentation_info,
      mojo::PendingRemote<PresentationConnection> controller_connection_remote,
      mojo::PendingReceiver<PresentationConnection>
          receiver_connection_receiver);
  void RemovePresentation(const std::string& presentation_id);

  const base::small_map<std::map<std::string, MediaRoute>>&
  presentation_id_to_route() const {
    return presentation_id_to_route_;
  }

 private:
  base::small_map<std::map<std::string, MediaRoute>> presentation_id_to_route_;
  base::small_map<
      std::map<std::string, std::unique_ptr<PresentationMediaSinksObserver>>>
      url_to_sinks_observer_;
  std::unordered_map<MediaRoute::Id, base::CallbackListSubscription>
      connection_state_subscriptions_;
  std::unordered_map<MediaRoute::Id,
                     std::unique_ptr<BrowserPresentationConnectionProxy>>
      browser_connection_proxies_;

  content::GlobalRenderFrameHostId render_frame_host_id_;

  // References to the owning WebContents, and the corresponding MediaRouter.
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<MediaRouter> router_;
};

PresentationFrame::PresentationFrame(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    content::WebContents* web_contents,
    MediaRouter* router)
    : render_frame_host_id_(render_frame_host_id),
      web_contents_(web_contents),
      router_(router) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationFrame::~PresentationFrame() = default;

MediaRoute::Id PresentationFrame::GetRouteId(
    const std::string& presentation_id) const {
  auto it = presentation_id_to_route_.find(presentation_id);
  return it != presentation_id_to_route_.end() ? it->second.media_route_id()
                                               : "";
}

bool PresentationFrame::SetScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  GURL url = listener->GetAvailabilityUrl();
  if (!IsValidPresentationUrl(url)) {
    listener->OnScreenAvailabilityChanged(
        ScreenAvailability::SOURCE_NOT_SUPPORTED);
    return false;
  }

  MediaRouterMetrics::RecordPresentationUrlType(url);

  MediaSource source = MediaSource::ForPresentationUrl(url);
  auto& sinks_observer = url_to_sinks_observer_[source.id()];
  if (sinks_observer && sinks_observer->listener() == listener)
    return false;

  sinks_observer = std::make_unique<PresentationMediaSinksObserver>(
      router_, listener, source,
      GetLastCommittedURLForFrame(render_frame_host_id_));

  if (!sinks_observer->Init()) {
    url_to_sinks_observer_.erase(source.id());
    listener->OnScreenAvailabilityChanged(ScreenAvailability::DISABLED);
    return false;
  }

  return true;
}

void PresentationFrame::RemoveScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  MediaSource source =
      MediaSource::ForPresentationUrl(listener->GetAvailabilityUrl());
  auto sinks_observer_it = url_to_sinks_observer_.find(source.id());
  if (sinks_observer_it != url_to_sinks_observer_.end() &&
      sinks_observer_it->second->listener() == listener) {
    url_to_sinks_observer_.erase(sinks_observer_it);
  }
}

bool PresentationFrame::HasScreenAvailabilityListenerForTest(
    const MediaSource::Id& source_id) const {
  return url_to_sinks_observer_.find(source_id) != url_to_sinks_observer_.end();
}

void PresentationFrame::Reset() {
  // Create a copy here to avoid `pid_route` being invalidated while iterating.
  // `router->DetachRoute()` might cause a Presentation route to be terminated
  // and removed from `presentation_id_to_route_`.
  auto presentation_id_to_route_copy(presentation_id_to_route_);
  for (const auto& pid_route : presentation_id_to_route_copy) {
    if (pid_route.second.is_local_presentation()) {
      auto* local_presentation_manager =
          LocalPresentationManagerFactory::GetOrCreateForWebContents(
              web_contents_);
      local_presentation_manager->UnregisterLocalPresentationController(
          pid_route.first, render_frame_host_id_);
    } else {
      // We avoid using `router_` here because it may have been invalidated if
      // this method is called during profile shutdown. This is a speculative
      // fix for crbug.com/1219904.
      MediaRouter* router = MediaRouterFactory::GetApiForBrowserContextIfExists(
          web_contents_->GetBrowserContext());
      if (router) {
        router->DetachRoute(pid_route.second.media_route_id());
      }
    }
  }

  presentation_id_to_route_.clear();
  url_to_sinks_observer_.clear();
  connection_state_subscriptions_.clear();
  browser_connection_proxies_.clear();
}

void PresentationFrame::AddPresentation(
    const PresentationInfo& presentation_info,
    const MediaRoute& route) {
  presentation_id_to_route_.emplace(presentation_info.id, route);
}

void PresentationFrame::ConnectToPresentation(
    const PresentationInfo& presentation_info,
    mojo::PendingRemote<PresentationConnection> controller_connection_remote,
    mojo::PendingReceiver<PresentationConnection>
        receiver_connection_receiver) {
  const auto pid_route_it =
      presentation_id_to_route_.find(presentation_info.id);

  if (pid_route_it == presentation_id_to_route_.end()) {
    return;
  }

  if (pid_route_it->second.is_local_presentation()) {
    auto* local_presentation_manager =
        LocalPresentationManagerFactory::GetOrCreateForWebContents(
            web_contents_);
    local_presentation_manager->RegisterLocalPresentationController(
        presentation_info, render_frame_host_id_,
        std::move(controller_connection_remote),
        std::move(receiver_connection_receiver), pid_route_it->second);
  } else {
    MediaRoute::Id route_id = pid_route_it->second.media_route_id();
    if (base::Contains(browser_connection_proxies_, route_id)) {
      return;
    }

    auto* proxy = new BrowserPresentationConnectionProxy(
        router_, route_id, std::move(receiver_connection_receiver),
        std::move(controller_connection_remote));
    browser_connection_proxies_.emplace(route_id, base::WrapUnique(proxy));
  }
}

void PresentationFrame::RemovePresentation(const std::string& presentation_id) {
  // Remove the presentation id mapping so a later call to Reset is a no-op.
  auto it = presentation_id_to_route_.find(presentation_id);
  if (it == presentation_id_to_route_.end())
    return;

  auto route_id = it->second.media_route_id();
  presentation_id_to_route_.erase(presentation_id);
  browser_connection_proxies_.erase(route_id);
  // We keep the PresentationConnectionStateChangedCallback registered with MR
  // so the MRP can tell us when terminate() completed.
}

void PresentationFrame::ListenForConnectionStateChange(
    const PresentationInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  auto it = presentation_id_to_route_.find(connection.id);
  if (it == presentation_id_to_route_.end()) {
    return;
  }

  const MediaRoute::Id& route_id = it->second.media_route_id();
  if (connection_state_subscriptions_.find(route_id) !=
      connection_state_subscriptions_.end()) {
    return;
  }

  connection_state_subscriptions_.emplace(
      route_id, router_->AddPresentationConnectionStateChangedCallback(
                    route_id, state_changed_cb));
}

PresentationServiceDelegateImpl*
PresentationServiceDelegateImpl::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // CreateForWebContents does nothing if the delegate instance already exists.
  PresentationServiceDelegateImpl::CreateForWebContents(web_contents);
  return PresentationServiceDelegateImpl::FromWebContents(web_contents);
}

PresentationServiceDelegateImpl::PresentationServiceDelegateImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PresentationServiceDelegateImpl>(
          *web_contents),
      router_(MediaRouterFactory::GetApiForBrowserContext(
          web_contents->GetBrowserContext())) {
  DCHECK(router_);
}

PresentationServiceDelegateImpl::~PresentationServiceDelegateImpl() = default;

void PresentationServiceDelegateImpl::AddObserver(int render_process_id,
                                                  int render_frame_id,
                                                  DelegateObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(render_process_id, render_frame_id, observer);
}

void PresentationServiceDelegateImpl::RemoveObserver(int render_process_id,
                                                     int render_frame_id) {
  observers_.RemoveObserver(render_process_id, render_frame_id);
}

bool PresentationServiceDelegateImpl::AddScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  content::GlobalRenderFrameHostId render_frame_host_id(render_process_id,
                                                        render_frame_id);
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  return presentation_frame->SetScreenAvailabilityListener(listener);
}

void PresentationServiceDelegateImpl::RemoveScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  content::GlobalRenderFrameHostId render_frame_host_id(render_process_id,
                                                        render_frame_id);
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->RemoveScreenAvailabilityListener(listener);
}

void PresentationServiceDelegateImpl::Reset(int render_process_id,
                                            int render_frame_id) {
  content::GlobalRenderFrameHostId render_frame_host_id(render_process_id,
                                                        render_frame_id);
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end()) {
    it->second->Reset();
    presentation_frames_.erase(it);
  }

  if (default_presentation_request_ &&
      render_frame_host_id ==
          default_presentation_request_->render_frame_host_id) {
    ClearDefaultPresentationRequest();
  }
}

PresentationFrame* PresentationServiceDelegateImpl::GetOrAddPresentationFrame(
    const content::GlobalRenderFrameHostId& render_frame_host_id) {
  auto& presentation_frame = presentation_frames_[render_frame_host_id];
  if (!presentation_frame) {
    presentation_frame = std::make_unique<PresentationFrame>(
        render_frame_host_id, &GetWebContents(), router_);
  }
  return presentation_frame.get();
}

void PresentationServiceDelegateImpl::SetDefaultPresentationUrls(
    const content::PresentationRequest& request,
    content::DefaultPresentationConnectionCallback callback) {
  if (request.presentation_urls.empty()) {
    ClearDefaultPresentationRequest();
    return;
  }

  DCHECK(!callback.is_null());
  default_presentation_started_callback_ = std::move(callback);
  default_presentation_request_ = request;
  NotifyDefaultPresentationChanged(&request);
}

void PresentationServiceDelegateImpl::OnJoinRouteResponse(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    const GURL& presentation_url,
    const std::string& presentation_id,
    content::PresentationConnectionCallback success_cb,
    content::PresentationConnectionErrorCallback error_cb,
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  if (!result.route()) {
    std::move(error_cb).Run(PresentationError(
        PresentationErrorType::NO_PRESENTATION_FOUND, result.error()));
  } else {
    DCHECK_EQ(presentation_id, result.presentation_id());
    PresentationInfo presentation_info(presentation_url,
                                       result.presentation_id());
    AddPresentation(render_frame_host_id, presentation_info, *result.route());
    EnsurePresentationConnection(render_frame_host_id, presentation_info,
                                 &connection);
    std::move(success_cb)
        .Run(blink::mojom::PresentationConnectionResult::New(
            presentation_info.Clone(), std::move(connection->connection_remote),
            std::move(connection->connection_receiver)));
  }
}

void PresentationServiceDelegateImpl::OnStartPresentationSucceeded(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    content::PresentationConnectionCallback success_cb,
    const PresentationInfo& new_presentation_info,
    mojom::RoutePresentationConnectionPtr connection,
    const MediaRoute& route) {
  AddPresentation(render_frame_host_id, new_presentation_info, route);
  EnsurePresentationConnection(render_frame_host_id, new_presentation_info,
                               &connection);
  std::move(success_cb)
      .Run(blink::mojom::PresentationConnectionResult::New(
          new_presentation_info.Clone(),
          std::move(connection->connection_remote),
          std::move(connection->connection_receiver)));
}

void PresentationServiceDelegateImpl::AddPresentation(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    const PresentationInfo& presentation_info,
    const MediaRoute& route) {
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  presentation_frame->AddPresentation(presentation_info, route);
  NotifyMediaRoutesChanged();
}

void PresentationServiceDelegateImpl::RemovePresentation(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    const std::string& presentation_id) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->RemovePresentation(presentation_id);
  NotifyMediaRoutesChanged();
}

void PresentationServiceDelegateImpl::StartPresentation(
    const content::PresentationRequest& request,
    content::PresentationConnectionCallback success_cb,
    content::PresentationConnectionErrorCallback error_cb) {
  const auto& render_frame_host_id = request.render_frame_host_id;
  const auto& presentation_urls = request.presentation_urls;
  if (presentation_urls.empty()) {
    std::move(error_cb).Run(PresentationError(
        PresentationErrorType::UNKNOWN, "Invalid presentation arguments."));
    return;
  }
  if (!base::ranges::all_of(presentation_urls, IsValidPresentationUrl)) {
    std::move(error_cb).Run(
        PresentationError(PresentationErrorType::NO_PRESENTATION_FOUND,
                          "Invalid presentation URL."));
    return;
  }
  auto presentation_context = std::make_unique<StartPresentationContext>(
      request,
      base::BindOnce(
          &PresentationServiceDelegateImpl::OnStartPresentationSucceeded,
          weak_factory_.GetWeakPtr(), render_frame_host_id,
          std::move(success_cb)),
      std::move(error_cb));
  if (start_presentation_cb_) {
    start_presentation_cb_.Run(std::move(presentation_context));
    return;
  }
  MediaRouterDialogController* controller =
      MediaRouterDialogController::GetOrCreateForWebContents(&GetWebContents());
  controller->ShowMediaRouterDialogForPresentation(
      std::move(presentation_context));
}

void PresentationServiceDelegateImpl::ReconnectPresentation(
    const content::PresentationRequest& request,
    const std::string& presentation_id,
    content::PresentationConnectionCallback success_cb,
    content::PresentationConnectionErrorCallback error_cb) {
  const auto& presentation_urls = request.presentation_urls;
  const auto& render_frame_host_id = request.render_frame_host_id;
  if (presentation_urls.empty()) {
    std::move(error_cb).Run(
        PresentationError(PresentationErrorType::NO_PRESENTATION_FOUND,
                          "Invalid presentation arguments."));
    return;
  }

  auto* local_presentation_manager =
      LocalPresentationManagerFactory::GetOrCreateForWebContents(
          &GetWebContents());
  // Check local presentation across frames.
  if (local_presentation_manager->IsLocalPresentation(presentation_id)) {
    auto* route = local_presentation_manager->GetRoute(presentation_id);

    if (!route ||
        !base::Contains(presentation_urls, route->media_source().url())) {
      return;
    }

    auto result = RouteRequestResult::FromSuccess(*route, presentation_id);
    OnJoinRouteResponse(render_frame_host_id, presentation_urls[0],
                        presentation_id, std::move(success_cb),
                        std::move(error_cb),
                        mojom::RoutePresentationConnectionPtr(), *result);
  } else {
    // TODO(crbug.com/1418744): Handle multiple URLs.
    const GURL& presentation_url = presentation_urls[0];
    router_->JoinRoute(
        MediaSource::ForPresentationUrl(presentation_url).id(), presentation_id,
        request.frame_origin, &GetWebContents(),
        base::BindOnce(&PresentationServiceDelegateImpl::OnJoinRouteResponse,
                       weak_factory_.GetWeakPtr(), render_frame_host_id,
                       presentation_url, presentation_id, std::move(success_cb),
                       std::move(error_cb)),
        base::TimeDelta());
  }
}

void PresentationServiceDelegateImpl::CloseConnection(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const content::GlobalRenderFrameHostId rfh_id(render_process_id,
                                                render_frame_id);
  auto route_id = GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    return;
  }

  auto* local_presentation_manager =
      LocalPresentationManagerFactory::GetOrCreateForWebContents(
          &GetWebContents());

  if (local_presentation_manager->IsLocalPresentation(presentation_id)) {
    local_presentation_manager->UnregisterLocalPresentationController(
        presentation_id, rfh_id);
  } else {
    router_->DetachRoute(route_id);
  }
  RemovePresentation(rfh_id, presentation_id);
  // TODO(mfoltz): close() should always succeed so there is no need to keep the
  // state_changed_cb around - remove it and fire the ChangeEvent on the
  // PresentationConnection in Blink.
}

void PresentationServiceDelegateImpl::Terminate(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const content::GlobalRenderFrameHostId rfh_id(render_process_id,
                                                render_frame_id);
  auto route_id = GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    return;
  }
  router_->TerminateRoute(route_id);
  RemovePresentation(rfh_id, presentation_id);
}

void PresentationServiceDelegateImpl::ListenForConnectionStateChange(
    int render_process_id,
    int render_frame_id,
    const PresentationInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  content::GlobalRenderFrameHostId render_frame_host_id(render_process_id,
                                                        render_frame_id);
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->ListenForConnectionStateChange(
        connection,
        base::BindRepeating(
            &PresentationServiceDelegateImpl::OnConnectionStateChanged,
            weak_factory_.GetWeakPtr(), render_frame_host_id, connection,
            state_changed_cb));
}

void PresentationServiceDelegateImpl::AddObserver(
    content::PresentationObserver* observer) {
  presentation_observers_.AddObserver(observer);
}

void PresentationServiceDelegateImpl::RemoveObserver(
    content::PresentationObserver* observer) {
  presentation_observers_.RemoveObserver(observer);
}

bool PresentationServiceDelegateImpl::HasDefaultPresentationRequest() const {
  return default_presentation_request_.has_value();
}

const content::PresentationRequest&
PresentationServiceDelegateImpl::GetDefaultPresentationRequest() const {
  DCHECK(HasDefaultPresentationRequest());
  return *default_presentation_request_;
}

void PresentationServiceDelegateImpl::OnPresentationResponse(
    const content::PresentationRequest& presentation_request,
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  if (!result.route() || !base::Contains(presentation_request.presentation_urls,
                                         result.presentation_url())) {
    return;
  }

  PresentationInfo presentation_info(result.presentation_url(),
                                     result.presentation_id());
  AddPresentation(presentation_request.render_frame_host_id, presentation_info,
                  *result.route());
  if (default_presentation_request_ &&
      ArePresentationRequestsEqual(*default_presentation_request_,
                                   presentation_request)) {
    EnsurePresentationConnection(presentation_request.render_frame_host_id,
                                 presentation_info, &connection);
    default_presentation_started_callback_.Run(
        blink::mojom::PresentationConnectionResult::New(
            presentation_info.Clone(), std::move(connection->connection_remote),
            std::move(connection->connection_receiver)));
  } else {
    DCHECK(!connection);
  }
}
std::vector<MediaRoute> PresentationServiceDelegateImpl::GetMediaRoutes() {
  std::vector<MediaRoute> routes;
  for (const auto& presentation_frame : presentation_frames_) {
    for (const auto& route :
         presentation_frame.second->presentation_id_to_route()) {
      routes.push_back(route.second);
    }
  }
  return routes;
}

base::WeakPtr<WebContentsPresentationManager>
PresentationServiceDelegateImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool PresentationServiceDelegateImpl::HasScreenAvailabilityListenerForTest(
    int render_process_id,
    int render_frame_id,
    const MediaSource::Id& source_id) const {
  content::GlobalRenderFrameHostId render_frame_host_id(render_process_id,
                                                        render_frame_id);
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end() &&
         it->second->HasScreenAvailabilityListenerForTest(source_id);
}

void PresentationServiceDelegateImpl::ClearDefaultPresentationRequest() {
  default_presentation_started_callback_.Reset();
  if (!default_presentation_request_)
    return;

  default_presentation_request_.reset();
  NotifyDefaultPresentationChanged(nullptr);
}

std::unique_ptr<media::FlingingController>
PresentationServiceDelegateImpl::GetFlingingController(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const content::GlobalRenderFrameHostId rfh_id(render_process_id,
                                                render_frame_id);
  MediaRoute::Id route_id = GetRouteId(rfh_id, presentation_id);

  if (route_id.empty())
    return nullptr;

  return router_->GetFlingingController(route_id);
}

MediaRoute::Id PresentationServiceDelegateImpl::GetRouteId(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    const std::string& presentation_id) const {
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end()
             ? it->second->GetRouteId(presentation_id)
             : MediaRoute::Id();
}

void PresentationServiceDelegateImpl::EnsurePresentationConnection(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    const PresentationInfo& presentation_info,
    mojom::RoutePresentationConnectionPtr* connection) {
  // This is where we ensure we have a valid Mojo pipe pair when starting or
  // joining a presentation.  If the MRP chose not return a pair of pipes
  // directly, we need to provide a BrowserPresentationConnectionProxy here or
  // connect to the LocalPresentationManager, as necessary.
  if (!*connection) {
    mojo::PendingRemote<PresentationConnection> controller_remote;
    mojo::PendingRemote<PresentationConnection> receiver_remote;
    *connection = mojom::RoutePresentationConnection::New(
        std::move(receiver_remote),
        controller_remote.InitWithNewPipeAndPassReceiver());
    auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
    presentation_frame->ConnectToPresentation(
        presentation_info, std::move(controller_remote),
        (*connection)->connection_remote.InitWithNewPipeAndPassReceiver());
  }
}

void PresentationServiceDelegateImpl::NotifyDefaultPresentationChanged(
    const content::PresentationRequest* request) {
  for (auto& presentation_observer : presentation_observers_)
    presentation_observer.OnDefaultPresentationChanged(request);
}

void PresentationServiceDelegateImpl::NotifyMediaRoutesChanged() {
  auto routes = GetMediaRoutes();

  for (auto& presentation_observer : presentation_observers_)
    presentation_observer.OnPresentationsChanged(!routes.empty());
}

void PresentationServiceDelegateImpl::OnConnectionStateChanged(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    const PresentationInfo& connection,
    const content::PresentationConnectionStateChangedCallback& state_changed_cb,
    const content::PresentationConnectionStateChangeInfo& info) {
  if (info.state == blink::mojom::PresentationConnectionState::CLOSED ||
      info.state == blink::mojom::PresentationConnectionState::TERMINATED) {
    RemovePresentation(render_frame_host_id, connection.id);
  }

  state_changed_cb.Run(info);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PresentationServiceDelegateImpl);

}  // namespace media_router
