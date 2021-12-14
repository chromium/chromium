// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_service.h"

#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"

namespace chromecast {

namespace {

class CastRuntimeServiceDummy : public CastRuntimeService {
 public:
  ~CastRuntimeServiceDummy() override = default;

  // CastRuntimeService implementation:
  WebCryptoServer* GetWebCryptoServer() override { return nullptr; }
  receiver::MediaManager* GetMediaManager() override { return nullptr; }
  CastWebService* GetCastWebService() override { return nullptr; }
  RuntimeApplication* GetRuntimeApplication() override { return nullptr; }

  // CastService implementation:
  void InitializeInternal() override {}
  void FinalizeInternal() override {}
  void StartInternal() override {}
  void StopInternal() override {}

  // CastRuntimeAudioChannelEndpointManager implementation:
  const std::string& GetAudioChannelEndpoint() override {
    static const std::string kFakeAudioChannelEndpoint = "";
    return kFakeAudioChannelEndpoint;
  }
};

}  // namespace

// static
std::unique_ptr<CastRuntimeService> CastRuntimeService::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CastWebService* web_service,
    media::MediaPipelineBackendManager* media_pipeline_backend_manager,
    CastRuntimeService::NetworkContextGetter network_context_getter,
    PrefService* pref_service,
    media::VideoPlaneController* video_plane_controller) {
  return std::make_unique<CastRuntimeServiceDummy>();
}

}  // namespace chromecast
