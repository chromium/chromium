// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/cma_backend_factory_impl.h"

#include "base/task/sequenced_task_runner.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/common/media_pipeline_backend_manager.h"
#if BUILDFLAG(ENABLE_CHROMIUM_RUNTIME_CAST_RENDERER)
#include "chromecast/media/cma/backend/proxy/cma_backend_proxy.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CHROMIUM_RUNTIME_CAST_RENDERER)
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

CmaBackendFactoryImpl::CmaBackendFactoryImpl(
    MediaPipelineBackendManager* media_pipeline_backend_manager)
    : media_pipeline_backend_manager_(media_pipeline_backend_manager) {
  DCHECK(media_pipeline_backend_manager_);
}

CmaBackendFactoryImpl::~CmaBackendFactoryImpl() = default;

std::unique_ptr<CmaBackend> CmaBackendFactoryImpl::CreateBackend(
    const MediaPipelineDeviceParams& params) {
  std::unique_ptr<CmaBackend> backend =
      media_pipeline_backend_manager_->CreateBackend(params);

#if BUILDFLAG(ENABLE_CHROMIUM_RUNTIME_CAST_RENDERER)
  backend = std::make_unique<CmaBackendProxy>(params, std::move(backend));
#endif  // BUILDFLAG(ENABLE_CHROMIUM_RUNTIME_CAST_RENDERER)

  return backend;
}

scoped_refptr<base::SequencedTaskRunner>
CmaBackendFactoryImpl::GetMediaTaskRunner() {
  return media_pipeline_backend_manager_->GetMediaTaskRunner();
}

}  // namespace media
}  // namespace chromecast
