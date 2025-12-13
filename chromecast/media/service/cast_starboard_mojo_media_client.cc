// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/service/cast_starboard_mojo_media_client.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/renderer/starboard_renderer.h"

namespace chromecast {
namespace media {

class CmaBackendFactory;
class VideoGeometrySetterService;
class VideoModeSwitcher;
class VideoResolutionPolicy;

CastStarboardMojoMediaClient::CastStarboardMojoMediaClient(
    CreateCdmFactoryCB create_cdm_factory_cb,
    EnableBufferingCB enable_buffering_cb,
    VideoGeometrySetterService* video_geometry_setter)
    : create_cdm_factory_cb_(std::move(create_cdm_factory_cb)),
      enable_buffering_cb_(std::move(enable_buffering_cb)),
      video_geometry_setter_(video_geometry_setter) {
  CHECK(create_cdm_factory_cb_);
  CHECK(enable_buffering_cb_);
  CHECK(video_geometry_setter_);
}

CastStarboardMojoMediaClient::~CastStarboardMojoMediaClient() = default;

std::unique_ptr<::media::Renderer>
CastStarboardMojoMediaClient::CreateCastRenderer(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ::media::MediaLog* /*media_log*/,
    const base::UnguessableToken& overlay_plane_id) {
  LOG(INFO) << "Using StarboardRenderer";
  return std::make_unique<StarboardRenderer>(
      chromecast::media::GetStarboardApiWrapper(), std::move(task_runner),
      overlay_plane_id, enable_buffering_cb_.Run(), video_geometry_setter_,
      chromecast::metrics::CastMetricsHelper::GetInstance());
}

std::unique_ptr<::media::Renderer> CastStarboardMojoMediaClient::CreateRenderer(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ::media::MediaLog* media_log,
    const std::string& audio_device_id) {
  // InterfaceFactoryImpl::CreateCastRenderer calls the cast-specific
  // MojoMediaClient::CreateCastRenderer instead of
  // MojoMediaClient::CreateRenderer. This is because cast needs access to the
  // overlay plane ID.
  NOTREACHED();
}

std::unique_ptr<::media::CdmFactory>
CastStarboardMojoMediaClient::CreateCdmFactory(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces) {
  return create_cdm_factory_cb_.Run(frame_interfaces);
}

}  // namespace media
}  // namespace chromecast
