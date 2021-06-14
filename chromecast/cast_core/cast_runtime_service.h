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

namespace content {
class BrowserContext;
}  // namespace content

namespace chromecast {

class CastWindowManager;
class WebCryptoServer;

namespace media {
class MediaPipelineBackendManager;
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
  static std::unique_ptr<CastRuntimeService> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      content::BrowserContext* browser_context,
      CastWindowManager* window_manager,
      media::MediaPipelineBackendManager* media_pipeline_backend_manager,
      PrefService* pref_service);

  // Returns current instance of CastRuntimeService in the browser process.
  static CastRuntimeService* GetInstance();

  CastRuntimeService();
  ~CastRuntimeService() override;

  virtual WebCryptoServer* GetWebCryptoServer();
  virtual receiver::MediaManager* GetMediaManager();

  // CastService overrides.
  void InitializeInternal() override;
  void FinalizeInternal() override;
  void StartInternal() override;
  void StopInternal() override;

  // CastRuntimeAudioChannelEndpointManager overrides.
  const std::string& GetAudioChannelEndpoint() override;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_CAST_RUNTIME_SERVICE_H_
