// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_H_
#define CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chromecast/media/cma/backend/proxy/cast_runtime_audio_channel_endpoint_manager.h"
#include "chromecast/service/cast_service.h"

class PrefService;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace chromecast {

class CastWebService;
class WebCryptoServer;
class RuntimeApplication;

namespace media {
class MediaPipelineBackendManager;
class VideoPlaneController;
}  // namespace media

namespace receiver {
class MediaManager;
}  // namespace receiver

// This interface is to be used for building the Cast Runtime Service and act as
// the border between shared Chromium code and the specifics of that
// implementation.
class CastRuntimeService
    : public CastService,
      public media::CastRuntimeAudioChannelEndpointManager {
 public:
  using NetworkContextGetter =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;

  // Creates an instance of |CastRuntimeService| interface.
  static std::unique_ptr<CastRuntimeService> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      CastWebService* web_service,
      media::MediaPipelineBackendManager* media_pipeline_backend_manager,
      NetworkContextGetter network_context_getter,
      PrefService* pref_service,
      media::VideoPlaneController* video_plane_controller);

  ~CastRuntimeService() override;

  // Returns WebCryptoServer.
  virtual WebCryptoServer* GetWebCryptoServer() = 0;

  // Returns MediaManager.
  virtual receiver::MediaManager* GetMediaManager() = 0;

  // Returns a pointer to CastWebService object with lifespan
  // equal to CastRuntimeService main object.
  virtual CastWebService* GetCastWebService() = 0;

  // Returns a pointer to RuntimeApplication.
  virtual RuntimeApplication* GetRuntimeApplication() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_H_
