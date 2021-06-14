// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_service.h"

#include "base/single_thread_task_runner.h"

namespace chromecast {

// static
std::unique_ptr<CastRuntimeService> CastRuntimeService::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    content::BrowserContext* browser_context,
    CastWindowManager* window_manager,
    media::MediaPipelineBackendManager* media_pipeline_backend_manager,
    PrefService* pref_service) {
  return std::make_unique<CastRuntimeService>();
}

}  // namespace chromecast
