// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_service.h"

#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"

namespace chromecast {

// static
std::unique_ptr<CastRuntimeService> CastRuntimeService::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CastWebService* web_service,
    media::MediaPipelineBackendManager* media_pipeline_backend_manager,
    CastRuntimeService::NetworkContextGetter network_context_getter,
    PrefService* pref_service,
    media::VideoPlaneController* video_plane_controller) {
  return std::make_unique<CastRuntimeService>();
}

CastRuntimeService* CastRuntimeService::GetInstance() {
  // TODO(b/186668532): Instead use the CastService singleton instead of
  // creating a new one with NoDestructor.
  static base::NoDestructor<CastRuntimeService> g_instance;
  return g_instance.get();
}

}  // namespace chromecast
