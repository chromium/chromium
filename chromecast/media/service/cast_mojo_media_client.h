// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_
#define CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <string>

#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/services/mojo_media_client.h"

namespace chromecast {
namespace media {

class CmaBackendFactory;
class VideoGeometrySetterService;
class VideoModeSwitcher;
class VideoResolutionPolicy;

class CastMojoMediaClient : public ::media::MojoMediaClient {
 public:
  using CreateCdmFactoryCB =
      base::RepeatingCallback<std::unique_ptr<::media::CdmFactory>(
          ::media::mojom::FrameInterfaceFactory*)>;
  using EnableBufferingCB = base::RepeatingCallback<bool()>;

  CastMojoMediaClient(CmaBackendFactory* backend_factory,
                      const CreateCdmFactoryCB& create_cdm_factory_cb,
                      VideoModeSwitcher* video_mode_switcher,
                      VideoResolutionPolicy* video_resolution_policy,
                      EnableBufferingCB enable_buffering_cb);

  CastMojoMediaClient(const CastMojoMediaClient&) = delete;
  CastMojoMediaClient& operator=(const CastMojoMediaClient&) = delete;

  ~CastMojoMediaClient() override;

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void SetVideoGeometrySetterService(
      VideoGeometrySetterService* video_geometry_setter);
#endif

  // MojoMediaClient implementation:
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  std::unique_ptr<::media::Renderer> CreateCastRenderer(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ::media::MediaLog* media_log,
      const base::UnguessableToken& overlay_plane_id) override;
#endif
  std::unique_ptr<::media::Renderer> CreateRenderer(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ::media::MediaLog* media_log,
      const std::string& audio_device_id) override;
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces) override;

 private:
  CmaBackendFactory* const backend_factory_;
  const CreateCdmFactoryCB create_cdm_factory_cb_;
  [[maybe_unused]] VideoModeSwitcher* video_mode_switcher_;
  [[maybe_unused]] VideoResolutionPolicy* video_resolution_policy_;
  const EnableBufferingCB enable_buffering_cb_;

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  VideoGeometrySetterService* video_geometry_setter_;
#endif
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_
