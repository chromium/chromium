// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/service/cast_mojo_media_client.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/api/cma_backend_factory.h"
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
    VideoResolutionPolicy* video_resolution_policy,
    CastMojoMediaClient::EnableBufferingCB enable_buffering_cb)
    : backend_factory_(backend_factory),
      create_cdm_factory_cb_(create_cdm_factory_cb),
      video_mode_switcher_(video_mode_switcher),
      video_resolution_policy_(video_resolution_policy),
      enable_buffering_cb_(std::move(enable_buffering_cb)) {
  DCHECK(backend_factory_);
}

CastMojoMediaClient::~CastMojoMediaClient() = default;

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void CastMojoMediaClient::SetVideoGeometrySetterService(
    VideoGeometrySetterService* video_geometry_setter) {
  video_geometry_setter_ = video_geometry_setter;
}

std::unique_ptr<::media::Renderer> CastMojoMediaClient::CreateCastRenderer(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ::media::MediaLog* /* media_log */,
    const base::UnguessableToken& overlay_plane_id) {
  DCHECK(video_geometry_setter_);
  auto cast_renderer = std::make_unique<CastRenderer>(
      backend_factory_, task_runner, video_mode_switcher_,
      video_resolution_policy_, overlay_plane_id, frame_interfaces,
      enable_buffering_cb_.Run());
  cast_renderer->SetVideoGeometrySetterService(video_geometry_setter_);
  return cast_renderer;
}
#endif

std::unique_ptr<::media::Renderer> CastMojoMediaClient::CreateRenderer(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ::media::MediaLog* /* media_log */,
    const std::string& audio_device_id) {
  // TODO(guohuideng): CastMojoMediaClient is used only when build flag
  // ENABLE_CAST_RENDERER is set. We can get rid of a number of related macros
  // and the [[maybe_unused]].
  NOTREACHED();
}

std::unique_ptr<::media::CdmFactory> CastMojoMediaClient::CreateCdmFactory(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces) {
  return create_cdm_factory_cb_.Run(frame_interfaces);
}

}  // namespace media
}  // namespace chromecast
