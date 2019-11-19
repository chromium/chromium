// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/service/cast_mojo_media_client.h"

#include "chromecast/media/cma/backend/cma_backend_factory.h"
#include "chromecast/media/service/cast_renderer.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/overlay_info.h"

namespace chromecast {
namespace media {

CastMojoMediaClient::CastMojoMediaClient(
    CmaBackendFactory* backend_factory,
    const CreateCdmFactoryCB& create_cdm_factory_cb,
    VideoModeSwitcher* video_mode_switcher,
    VideoResolutionPolicy* video_resolution_policy)
    : connector_(nullptr),
      backend_factory_(backend_factory),
      create_cdm_factory_cb_(create_cdm_factory_cb),
      video_mode_switcher_(video_mode_switcher),
      video_resolution_policy_(video_resolution_policy) {
  DCHECK(backend_factory_);
}

CastMojoMediaClient::~CastMojoMediaClient() {}

void CastMojoMediaClient::Initialize(service_manager::Connector* connector) {
  DCHECK(!connector_);
  DCHECK(connector);
  connector_ = connector;
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void CastMojoMediaClient::SetVideoGeometrySetterService(
    VideoGeometrySetterService* video_geometry_setter) {
  video_geometry_setter_ = video_geometry_setter;
}

std::unique_ptr<::media::Renderer> CastMojoMediaClient::CreateCastRenderer(
    service_manager::mojom::InterfaceProvider* host_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ::media::MediaLog* /* media_log */,
    const base::UnguessableToken& overlay_plane_id) {
  DCHECK(video_geometry_setter_);
  auto cast_renderer = std::make_unique<CastRenderer>(
      backend_factory_, task_runner, video_mode_switcher_,
      video_resolution_policy_, overlay_plane_id, connector_, host_interfaces);
  cast_renderer->SetVideoGeometrySetterService(video_geometry_setter_);
  return cast_renderer;
}
#endif

std::unique_ptr<::media::Renderer> CastMojoMediaClient::CreateRenderer(
    service_manager::mojom::InterfaceProvider* host_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ::media::MediaLog* /* media_log */,
    const std::string& audio_device_id) {
  // TODO(guohuideng): CastMojoMediaClient is used only when build flag
  // ENABLE_CAST_RENDERER is set. We can get rid of a number of related macros
  // And the ANALYZER_ALLOW_UNUSED below.
  ANALYZER_ALLOW_UNUSED(video_mode_switcher_);
  ANALYZER_ALLOW_UNUSED(video_resolution_policy_);
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<::media::CdmFactory> CastMojoMediaClient::CreateCdmFactory(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  return create_cdm_factory_cb_.Run(host_interfaces);
}

}  // namespace media
}  // namespace chromecast
