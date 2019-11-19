// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_
#define CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <string>

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
          service_manager::mojom::InterfaceProvider*)>;

  CastMojoMediaClient(CmaBackendFactory* backend_factory,
                      const CreateCdmFactoryCB& create_cdm_factory_cb,
                      VideoModeSwitcher* video_mode_switcher,
                      VideoResolutionPolicy* video_resolution_policy);
  ~CastMojoMediaClient() override;

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void SetVideoGeometrySetterService(
      VideoGeometrySetterService* video_geometry_setter);
#endif

  // MojoMediaClient implementation:
  void Initialize(service_manager::Connector* connector) override;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  std::unique_ptr<::media::Renderer> CreateCastRenderer(
      service_manager::mojom::InterfaceProvider* host_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ::media::MediaLog* media_log,
      const base::UnguessableToken& overlay_plane_id) override;
#endif
  std::unique_ptr<::media::Renderer> CreateRenderer(
      service_manager::mojom::InterfaceProvider* host_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ::media::MediaLog* media_log,
      const std::string& audio_device_id) override;
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override;

 private:
  service_manager::Connector* connector_;
  CmaBackendFactory* const backend_factory_;
  const CreateCdmFactoryCB create_cdm_factory_cb_;
  VideoModeSwitcher* video_mode_switcher_;
  VideoResolutionPolicy* video_resolution_policy_;

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  VideoGeometrySetterService* video_geometry_setter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CastMojoMediaClient);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_CAST_MOJO_MEDIA_CLIENT_H_
