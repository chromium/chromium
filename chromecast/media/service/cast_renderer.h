// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_SERVICE_CAST_RENDERER_H_
#define CHROMECAST_MEDIA_SERVICE_CAST_RENDERER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "chromecast/media/base/video_resolution_policy.h"
#include "chromecast/media/cma/backend/cma_backend_factory.h"
#include "chromecast/media/service/mojom/video_geometry_setter.mojom.h"
#include "media/base/renderer.h"
#include "media/base/waiting.h"
#include "media/mojo/mojom/cast_application_media_info_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace service_manager {
class Connector;
namespace mojom {
class InterfaceProvider;
}  // namespace mojom
}  // namespace service_manager

namespace chromecast {
class TaskRunnerImpl;

namespace media {
class BalancedMediaTaskRunnerFactory;
class CastCdmContext;
class MediaPipelineImpl;
class VideoGeometrySetterService;
class VideoModeSwitcher;

class CastRenderer : public ::media::Renderer,
                     public VideoResolutionPolicy::Observer,
                     public mojom::VideoGeometryChangeClient {
 public:
  // |connector| provides interfaces for services hosted by ServiceManager.
  // |host_interfaces| provides interfaces tied to RenderFrameHost.
  CastRenderer(CmaBackendFactory* backend_factory,
               const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
               VideoModeSwitcher* video_mode_switcher,
               VideoResolutionPolicy* video_resolution_policy,
               const base::UnguessableToken& overlay_plane_id,
               service_manager::Connector* connector,
               service_manager::mojom::InterfaceProvider* host_interfaces);
  ~CastRenderer() final;
  // For CmaBackend implementation, CastRenderer must be connected to
  // VideoGeometrySetterService.
  void SetVideoGeometrySetterService(
      VideoGeometrySetterService* video_geometry_setter_service);

  // ::media::Renderer implementation.
  void Initialize(::media::MediaResource* media_resource,
                  ::media::RendererClient* client,
                  ::media::PipelineStatusCallback init_cb) final;
  void SetCdm(::media::CdmContext* cdm_context,
              ::media::CdmAttachedCB cdm_attached_cb) final;
  void Flush(base::OnceClosure flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;

  // VideoResolutionPolicy::Observer implementation.
  void OnVideoResolutionPolicyChanged() override;

  // mojom::VideoGeometryChangeClient implementation.
  void OnVideoGeometryChange(const gfx::RectF& rect_f,
                             gfx::OverlayTransform transform) final;

  // TODO(guohuideng): For now we use a global callback to gain access to
  // VideoPlaneController so CastRenderer can set the video geometry. We
  // should separate the SetGeometry from VideoPlaneController and get rid
  // of this callback. see b/79266094.
  using OverlayCompositedCallback =
      base::RepeatingCallback<void(const gfx::RectF&, gfx::OverlayTransform)>;
  static void SetOverlayCompositedCallback(const OverlayCompositedCallback& cb);

 private:
  enum Stream { STREAM_AUDIO, STREAM_VIDEO };
  void OnSubscribeToVideoGeometryChange(::media::MediaResource* media_resource,
                                        ::media::RendererClient* client);
  void OnApplicationMediaInfoReceived(
      ::media::MediaResource* media_resource,
      ::media::RendererClient* client,
      ::media::mojom::CastApplicationMediaInfoPtr application_media_info);
  void OnGetMultiroomInfo(
      ::media::MediaResource* media_resource,
      ::media::RendererClient* client,
      ::media::mojom::CastApplicationMediaInfoPtr application_media_info,
      chromecast::mojom::MultiroomInfoPtr multiroom_info);
  void OnError(::media::PipelineStatus status);
  void OnEnded(Stream stream);
  void OnStatisticsUpdate(const ::media::PipelineStatistics& stats);
  void OnBufferingStateChange(::media::BufferingState state,
                              ::media::BufferingStateChangeReason reason);
  void OnWaiting(::media::WaitingReason reason);
  void OnVideoNaturalSizeChange(const gfx::Size& size);
  void OnVideoOpacityChange(bool opaque);
  void CheckVideoResolutionPolicy();
  void RunInitCallback(::media::PipelineStatus status);
  void OnVideoInitializationFinished(::media::PipelineStatus status);

  CmaBackendFactory* const backend_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  VideoModeSwitcher* video_mode_switcher_;
  VideoResolutionPolicy* video_resolution_policy_;
  base::UnguessableToken overlay_plane_id_;
  service_manager::Connector* connector_;
  service_manager::mojom::InterfaceProvider* host_interfaces_;

  ::media::RendererClient* client_;
  CastCdmContext* cast_cdm_context_;
  scoped_refptr<BalancedMediaTaskRunnerFactory> media_task_runner_factory_;
  std::unique_ptr<TaskRunnerImpl> backend_task_runner_;
  std::unique_ptr<MediaPipelineImpl> pipeline_;
  bool eos_[2];
  gfx::Size video_res_;

  mojo::Remote<::media::mojom::CastApplicationMediaInfoManager>
      application_media_info_manager_remote_;
  VideoGeometrySetterService* video_geometry_setter_service_;
  mojo::Remote<mojom::VideoGeometryChangeSubscriber>
      video_geometry_change_subcriber_remote_;
  mojo::Remote<chromecast::mojom::MultiroomManager> multiroom_manager_;
  ::media::PipelineStatusCallback init_cb_;
  mojo::Receiver<mojom::VideoGeometryChangeClient>
      video_geometry_change_client_receiver_{this};

  static OverlayCompositedCallback& GetOverlayCompositedCallback() {
    static base::NoDestructor<OverlayCompositedCallback>
        g_overlay_composited_callback;
    return *g_overlay_composited_callback;
  }

  base::WeakPtrFactory<CastRenderer> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(CastRenderer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_SERVICE_CAST_RENDERER_H_
