// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_BROWSER_CAST_SERVICE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_BROWSER_CAST_SERVICE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder.h"
#include "chromecast/cast_core/runtime/browser/core_browser_cast_service.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"
#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_endpoint_manager.h"
#include "chromecast/service/cast_service.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace chromecast {

class CastWebService;
class WebCryptoServer;

namespace receiver {
class MediaManager;
}  // namespace receiver

namespace media {
class VideoPlaneController;
}  // namespace media

// This interface is to be used for building the Cast Runtime Service and act as
// the border between shared Chromium code and the specifics of that
// implementation.
class CoreBrowserCastService
    : public CastService,
      public CastRuntimeMetricsRecorder::EventBuilderFactory,
      public media::CastRuntimeAudioChannelEndpointManager {
 public:
  using NetworkContextGetter =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;

  CoreBrowserCastService(CastWebService* web_service,
                         NetworkContextGetter network_context_getter,
                         media::VideoPlaneController* video_plane_controller);

  // Flags if buffering is enabled.
  RuntimeApplicationDispatcher* app_dispatcher() { return &app_dispatcher_; }

  // Returns WebCryptoServer.
  virtual WebCryptoServer* GetWebCryptoServer();

  // Returns MediaManager.
  virtual receiver::MediaManager* GetMediaManager();

 private:
  // CastService implementation:
  void InitializeInternal() override;
  void FinalizeInternal() override;
  void StartInternal() override;
  void StopInternal() override;

  // CastRuntimeMetricsRecorder::EventBuilderFactory implementation:
  std::unique_ptr<CastEventBuilder> CreateEventBuilder() override;

  // CastRuntimeAudioChannelEndpointManager implementation:
  const std::string& GetAudioChannelEndpoint() override;

 private:
  RuntimeApplicationDispatcher app_dispatcher_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_BROWSER_CAST_SERVICE_H_
