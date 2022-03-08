// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_discovery_interface.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/discovery/access_code/discovery_resources.pb.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

using ::access_code_cast::mojom::AddSinkResultCode;
using ::media_router::AccessCodeCastDiscoveryInterface;
using ::media_router::CreateCastMediaSinkResult;
using ::media_router::MediaSinkInternal;

namespace content {
struct PresentationRequest;
class WebContents;
}  // namespace content

namespace media_router {
class MediaRouter;
}

// TODO(b/213324920): Remove WebUI from the media_router namespace after
// expiration module has been completed.
namespace media_router {

class AccessCodeCastHandler : public access_code_cast::mojom::PageHandler,
                              public QueryResultManager::Observer,
                              public WebContentsPresentationManager::Observer {
 public:
  using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

  AccessCodeCastHandler(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      Profile* profile,
      media_router::MediaRouter* media_router,
      const media_router::CastModeSet& cast_mode_set,
      content::WebContents* web_contents,
      std::unique_ptr<StartPresentationContext> start_presentation_context);

  // Constructor that is used for testing.
  AccessCodeCastHandler(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      Profile* profile,
      media_router::MediaRouter* media_router,
      const media_router::CastModeSet& cast_mode_set,
      content::WebContents* web_contents,
      std::unique_ptr<StartPresentationContext> start_presentation_context,
      AccessCodeCastSinkService* access_code_sink_service);

  ~AccessCodeCastHandler() override;

  // access_code_cast::mojom::PageHandler overrides:
  void AddSink(const std::string& access_code,
               access_code_cast::mojom::CastDiscoveryMethod discovery_method,
               AddSinkCallback callback) override;

  // access_code_cast::mojom::PageHandler overrides:
  void CastToSink(CastToSinkCallback callback) override;

 private:
  friend class AccessCodeCastHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest,
                           DiscoveryDeviceMissingWithOk);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest,
                           ValidDiscoveryDeviceAndCode);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, InvalidDiscoveryDevice);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, NonOKResultCode);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, DiscoveredDeviceAdded);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, OtherDevicesIgnored);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, DesktopMirroring);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, DesktopMirroringError);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, OnChannelOpened);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest,
                           OnChannelOpenedExistingSink);

  // Returns true if the specified cast mode is among the cast modes specified
  // for the dialog to use when it was initialized.
  bool IsCastModeAvailable(MediaCastMode mode) const;

  // Initialize the query manager with the various potential media sources based
  // on the suggested available cast modes.
  void InitMediaSources();
  // Add the various presentation sources to the QueryResultManager if
  // presentation mode is available.
  void InitPresentationSources();
  // Add the various mirroring sources to the QueryResultManager if the
  // requisite mirroring casting mode is available.
  void InitMirroringSources();

  void OnAccessCodeValidated(
      absl::optional<DiscoveryDevice> discovery_device,
      access_code_cast::mojom::AddSinkResultCode result_code);

  void OnChannelOpenedResult(bool channel_opened);

  // QueryResultManager::Observer:
  void OnResultsUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) override;

  // WebContentsPresentationManager::Observer
  void OnDefaultPresentationChanged(
      const content::PresentationRequest* presentation_request) override;

  // Method to remove presentation sources from the QueryResultManager
  void OnDefaultPresentationRemoved();

  // Callback passed to MediaRouter to receive response to route creation
  // requests.
  void OnRouteResponse(MediaCastMode cast_mode,
                       int route_request_id,
                       const MediaSink::Id& sink_id,
                       MediaRouteResponseCallback presentation_callback,
                       CastToSinkCallback dialog_callback,
                       mojom::RoutePresentationConnectionPtr connection,
                       const RouteRequestResult& result);

  // Populates common route-related parameters for calls to MediaRouter.
  absl::optional<RouteParameters> GetRouteParameters(MediaCastMode cast_mode);

  void SetSinkCallbackForTesting(AddSinkCallback callback);

  void set_sink_id_for_testing(const MediaSink::Id& sink_id) {
    sink_id_ = sink_id;
  }

  // Checks to see if all the conditions necessary to complete discovery have
  // been satisfied. If so, alerts the dialog.
  void CheckForDiscoveryCompletion();

  mojo::Remote<access_code_cast::mojom::Page> page_;
  mojo::Receiver<access_code_cast::mojom::PageHandler> receiver_;

  std::unique_ptr<AccessCodeCastDiscoveryInterface> discovery_server_interface_;

  // Used to fetch OAuth2 access tokens.
  raw_ptr<Profile> const profile_;

  const raw_ptr<media_router::MediaRouter> media_router_;
  const media_router::CastModeSet cast_mode_set_;
  const raw_ptr<content::WebContents> web_contents_;

  AddSinkCallback add_sink_callback_;

  // The id of the media sink discovered from the access code;
  absl::optional<MediaSink::Id> sink_id_;
  // Set of cast modes supported by the discovered sink;
  media_router::CastModeSet supported_cast_modes_;

  // Monitors and reports sink availability.
  std::unique_ptr<QueryResultManager> query_result_manager_;

  // Set to the presentation request corresponding to the presentation cast
  // mode, if supported. Otherwise set to nullopt.
  absl::optional<content::PresentationRequest> presentation_request_;

  // If set, then the result of the next presentation route request will
  // be handled by this object instead of |presentation_manager_|
  std::unique_ptr<StartPresentationContext> start_presentation_context_;

  // |presentation_manager_| notifies |this| whenever there is an update to the
  // default PresentationRequest or MediaRoutes associated with |web_contents_|.
  base::WeakPtr<WebContentsPresentationManager> presentation_manager_;

  // This contains a value only when tracking a pending route request.
  absl::optional<MediaRouterUI::RouteRequest> current_route_request_;

  raw_ptr<AccessCodeCastSinkService> const access_code_sink_service_;

  base::WeakPtrFactory<AccessCodeCastHandler> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_
