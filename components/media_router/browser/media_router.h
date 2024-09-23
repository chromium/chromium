// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "components/media_router/browser/presentation_connection_message_observer.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "media/base/flinging_controller.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/browser/media_router_debugger.h"
#include "components/media_router/common/mojom/media_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {
class WebContents;
}

namespace url {
class Origin;
}  // namespace url

namespace media_router {

class IssueManager;
class MediaRoutesObserver;
class MediaSinksObserver;
class PresentationConnectionStateObserver;
class RouteRequestResult;

// Commandline flag to disable the default media route providers for tests that
// are sensitive to the presence of sinks e.g. on the local network.
constexpr char kDisableMediaRouteProvidersForTestSwitch[] =
    "disable-media-route-providers-for-test";

// Type of callback used in |CreateRoute()| and |JoinRoute()|.  Callback is
// invoked when the route request either succeeded or failed.  |connection| is
// set depending on whether the MRP chooses to setup the PresentationConnections
// itself.
using MediaRouteResponseCallback =
    base::OnceCallback<void(mojom::RoutePresentationConnectionPtr connection,
                            const RouteRequestResult& result)>;

// An interface for handling resources related to media routing.
// Responsible for registering observers for receiving sink availability
// updates, handling route requests/responses, and operating on routes (e.g.
// posting messages or closing).
// TODO(imcheng): Reduce number of parameters by putting them into structs.
class MediaRouter : public KeyedService {
 public:
  ~MediaRouter() override = default;

  // Must be called before invoking any other method.
  virtual void Initialize() = 0;

  // Creates a media route from |source_id| to |sink_id|.
  // |origin| is the origin of requestor's page.
  // |web_contents| is the WebContents of the tab in which the request was made.
  // |origin| and |web_contents| are used for enforcing same-origin and/or
  // same-tab scope for JoinRoute() requests. (e.g., if enforced, the page
  // requesting JoinRoute() must have the same origin as the page that requested
  // CreateRoute()).
  // The caller may pass in nullptr for |web_contents| if tab is not applicable.
  // Each callback in |callbacks| is invoked with a response indicating
  // success or failure, in the order they are listed.
  // If |timeout| is positive, then any un-invoked |callbacks| will be invoked
  // with a timeout error after the timeout expires.
  virtual void CreateRoute(const MediaSource::Id& source_id,
                           const MediaSink::Id& sink_id,
                           const url::Origin& origin,
                           content::WebContents* web_contents,
                           MediaRouteResponseCallback callback,
                           base::TimeDelta timeout) = 0;

  // Joins an existing route identified by |presentation_id|.
  // |source|: The source to route to the existing route.
  // |presentation_id|: Presentation ID of the existing route.
  // |origin|, |web_contents|: Origin and WebContents of the join route request.
  // Used for validation when enforcing same-origin and/or same-tab scope.
  // (See CreateRoute documentation).
  // Each callback in |callbacks| is invoked with a response indicating
  // success or failure, in the order they are listed.
  // If |timeout| is positive, then any un-invoked |callbacks| will be invoked
  // with a timeout error after the timeout expires.
  virtual void JoinRoute(const MediaSource::Id& source,
                         const std::string& presentation_id,
                         const url::Origin& origin,
                         content::WebContents* web_contents,
                         MediaRouteResponseCallback callback,
                         base::TimeDelta timeout) = 0;

  // Terminates the media route specified by |route_id|.
  virtual void TerminateRoute(const MediaRoute::Id& route_id) = 0;

  // Detaches the media route specified by |route_id|. The request might come
  // from the page or from an event like navigation or garbage collection.
  virtual void DetachRoute(MediaRoute::Id route_id) = 0;

  // Posts |message| to a MediaSink connected via MediaRoute with |route_id|.
  virtual void SendRouteMessage(const MediaRoute::Id& route_id,
                                const std::string& message) = 0;

  // Sends |data| to a MediaSink connected via MediaRoute with |route_id|.
  // This is called for Blob / ArrayBuffer / ArrayBufferView types.
  virtual void SendRouteBinaryMessage(
      const MediaRoute::Id& route_id,
      std::unique_ptr<std::vector<uint8_t>> data) = 0;

  // Notifies the Media Router that the user has taken an action involving the
  // Media Router. This can be used to perform any initialization that is not
  // approriate to be done at construction.
  virtual void OnUserGesture() = 0;

  // Adds |callback| to listen for state changes for presentation connected to
  // |route_id|. The returned subscription is owned by the caller. |callback|
  // will be invoked whenever there are state changes, until the caller destroys
  // the subscription.
  virtual base::CallbackListSubscription
  AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback) = 0;

  // Returns the media routes that currently exist. To get notified whenever
  // there is a change to the media routes, subclass MediaRoutesObserver.
  virtual std::vector<MediaRoute> GetCurrentRoutes() const = 0;

  // Returns a controller that sends commands to media within a route, and
  // propagates MediaStatus changes.
  // Returns a nullptr if no controller can be be found from |route_id|.
  virtual std::unique_ptr<media::FlingingController> GetFlingingController(
      const MediaRoute::Id& route_id) = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Returns a pointer to a controller host that sends media commands related to
  // mirroring within a route.
  virtual MirroringMediaControllerHost* GetMirroringMediaControllerHost(
      const MediaRoute::Id& route_id) = 0;

  // Returns the IssueManager owned by the MediaRouter. Guaranteed to be
  // non-null.
  virtual IssueManager* GetIssueManager() = 0;

  // Binds |controller| for sending media commands to a route. The controller
  // will notify |observer| whenever there is a change to the status of the
  // media. It may invalidate bindings from previous calls to this method.
  virtual void GetMediaController(
      const MediaRoute::Id& route_id,
      mojo::PendingReceiver<mojom::MediaController> controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) = 0;

  // Returns logs collected from Media Router components.
  // Used by chrome://media-router-internals.
  virtual base::Value GetLogs() const = 0;

  // Returns media router state as a JSON string represented by base::Value.
  // Includes known sinks and sink compatibility with media sources.
  // Used by chrome://media-router-internals.
  virtual base::Value::Dict GetState() const = 0;

  // Returns the media route provider state for |provider_id| via |callback|.
  // Includes details about routes/sessions owned by the MRP.
  // Used by chrome://media-router-internals.
  virtual void GetProviderState(
      mojom::MediaRouteProviderId provider_id,
      mojom::MediaRouteProvider::GetStateCallback callback) const = 0;

  // Returns a pointer to LoggerImpl that can be used to add logging messages.
  virtual LoggerImpl* GetLogger() = 0;

  // Returns the instance of the debugger for this MediaRouter instance.
  virtual MediaRouterDebugger& GetDebugger() = 0;

#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  // TODO(crbug.com/40177419): remove message observer classes and API.
  friend class IssuesObserver;
  friend class MediaSinksObserver;
  friend class MediaRoutesObserver;
  friend class PresentationConnectionStateObserver;
  friend class PresentationConnectionMessageObserver;

  // The following functions are called by friend Observer classes above.

  // Registers |observer| with this MediaRouter. |observer| specifies a media
  // source and will receive updates with media sinks that are compatible with
  // that source. The initial update may happen synchronously.
  // NOTE: This class does not assume ownership of |observer|. Callers must
  // manage |observer| and make sure |UnregisterObserver()| is called
  // before the observer is destroyed.
  // It is invalid to register the same observer more than once and will result
  // in undefined behavior.
  // If the MRPM Host is not available, the registration request will fail
  // immediately.
  // The implementation can reject the request to observe in which case it will
  // notify the caller by returning |false|.
  virtual bool RegisterMediaSinksObserver(MediaSinksObserver* observer) = 0;

  // Removes a previously added MediaSinksObserver. |observer| will stop
  // receiving further updates.
  virtual void UnregisterMediaSinksObserver(MediaSinksObserver* observer) = 0;

  // Adds a MediaRoutesObserver to listen for updates on MediaRoutes.
  // The initial update may happen synchronously.
  // MediaRouter does not own |observer|. |UnregisterMediaRoutesObserver| should
  // be called before |observer| is destroyed.
  // It is invalid to register the same observer more than once and will result
  // in undefined behavior.
  virtual void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) = 0;

  // Removes a previously added MediaRoutesObserver. |observer| will stop
  // receiving further updates.
  virtual void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) = 0;

  // Registers |observer| with this MediaRouter. |observer| specifies a media
  // route and will receive messages from the MediaSink connected to the
  // route. Note that MediaRouter does not own |observer|. |observer| should be
  // unregistered before it is destroyed. Registering the same observer more
  // than once will result in undefined behavior.
  virtual void RegisterPresentationConnectionMessageObserver(
      PresentationConnectionMessageObserver* observer) = 0;

  // Unregisters a previously registered RouteMessagesObserver. |observer| will
  // stop receiving further updates.
  virtual void UnregisterPresentationConnectionMessageObserver(
      PresentationConnectionMessageObserver* observer) = 0;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_H_
