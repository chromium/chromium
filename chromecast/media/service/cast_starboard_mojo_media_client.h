// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_CAST_STARBOARD_MOJO_MEDIA_CLIENT_H_
#define CHROMECAST_MEDIA_SERVICE_CAST_STARBOARD_MOJO_MEDIA_CLIENT_H_

#include "base/unguessable_token.h"
#include "chromecast/media/service/create_mojo_media_client.h"
#include "media/mojo/services/mojo_media_client.h"

namespace chromecast {
namespace media {

class VideoGeometrySetterService;

// A mojo media client that creates StarboardRenderer objects. `CreateRenderer`
// should not be called; use `CreateCastRenderer` instead.
//
// This class is meant to replace CastMojoMediaClient for Starboard-based Linux
// cast builds. CastStarboardMojoMediaClient should be created via
// CreateMojoMediaClientForCast (declared in create_mojo_media_client.h).
class CastStarboardMojoMediaClient : public ::media::MojoMediaClient {
 public:
  CastStarboardMojoMediaClient(
      CreateCdmFactoryCB create_cdm_factory_cb,
      EnableBufferingCB enable_buffering_cb,
      VideoGeometrySetterService* video_geometry_setter);

  ~CastStarboardMojoMediaClient() override;

  // MojoMediaClient implementation:
  std::unique_ptr<::media::Renderer> CreateCastRenderer(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ::media::MediaLog* media_log,
      const base::UnguessableToken& overlay_plane_id) override;

  std::unique_ptr<::media::Renderer> CreateRenderer(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ::media::MediaLog* media_log,
      const std::string& audio_device_id) override;

  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces) override;

 private:
  CreateCdmFactoryCB create_cdm_factory_cb_;
  EnableBufferingCB enable_buffering_cb_;
  VideoGeometrySetterService* video_geometry_setter_ = nullptr;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_CAST_STARBOARD_MOJO_MEDIA_CLIENT_H_
