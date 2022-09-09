// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/discovery/access_code/discovery_resources.pb.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes_observer.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

using ::access_code_cast::mojom::AddSinkResultCode;
using ::media_router::AccessCodeCastDiscoveryInterface;
using ::media_router::CreateCastMediaSinkResult;
using ::media_router::MediaSinkInternal;

namespace media_router {
class MediaRouter;
}

// TODO(b/213324920): Remove WebUI from the media_router namespace after
// expiration module has been completed.
namespace media_router {

class AccessCodeCastHandler : public access_code_cast::mojom::PageHandler,
                              public MediaSinkWithCastModesObserver {
 public:
  using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

  AccessCodeCastHandler(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      const media_router::CastModeSet& cast_mode_set,
      std::unique_ptr<MediaRouteStarter> media_route_starter);

  ~AccessCodeCastHandler() override;

  // access_code_cast::mojom::PageHandler overrides:
  void AddSink(const std::string& access_code,
               access_code_cast::mojom::CastDiscoveryMethod discovery_method,
               AddSinkCallback callback) override;

  // access_code_cast::mojom::PageHandler overrides:
  void CastToSink(CastToSinkCallback callback) override;

 private:
  friend class AccessCodeCastHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, DiscoveredDeviceAdded);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, OtherDevicesIgnored);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, DesktopMirroring);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, DesktopMirroringError);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, OnSinkAddedResult);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, RouteAlreadyExists);

  // Constructor that is used for testing.
  AccessCodeCastHandler(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      const media_router::CastModeSet& cast_mode_set,
      std::unique_ptr<MediaRouteStarter> media_route_starter,
      AccessCodeCastSinkService* access_code_sink_service);

  void Init();

  // Returns true if the specified cast mode is among the cast modes specified
  // for the dialog to use when it was initialized.
  bool IsCastModeAvailable(MediaCastMode mode) const;

  MediaRouter* GetMediaRouter() const {
    return media_route_starter_->GetMediaRouter();
  }

  void OnSinkAddedResult(
      access_code_cast::mojom::AddSinkResultCode add_sink_result,
      absl::optional<MediaSink::Id> sink_id);

  // MediaSinkWithCastModesObserver:
  void OnSinksUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) override;

  // Callback passed to MediaRouter to receive response to route creation
  // requests.
  void OnRouteResponse(MediaCastMode cast_mode,
                       int route_request_id,
                       const MediaSink::Id& sink_id,
                       CastToSinkCallback dialog_callback,
                       const RouteRequestResult& result);

  void SetSinkCallbackForTesting(AddSinkCallback callback);

  void set_sink_id_for_testing(const MediaSink::Id& sink_id) {
    sink_id_ = sink_id;
  }

  // Checks to see if all the conditions necessary to complete discovery have
  // been satisfied. If so, alerts the dialog.
  void CheckForDiscoveryCompletion();

  // Checks to see that if route already exists for the given media sink id.
  bool HasActiveRoute(const MediaSink::Id& sink_id);

  mojo::Remote<access_code_cast::mojom::Page> page_;
  mojo::Receiver<access_code_cast::mojom::PageHandler> receiver_;

  const media_router::CastModeSet cast_mode_set_;

  // Contains the info necessary to start a media route.
  std::unique_ptr<MediaRouteStarter> media_route_starter_;

  raw_ptr<AccessCodeCastSinkService> access_code_sink_service_;

  AddSinkCallback add_sink_callback_;

  // The id of the media sink discovered from the access code;
  absl::optional<MediaSink::Id> sink_id_;

  // This contains a value only when tracking a pending route request.
  absl::optional<RouteRequest> current_route_request_;

  base::WeakPtrFactory<AccessCodeCastHandler> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_
