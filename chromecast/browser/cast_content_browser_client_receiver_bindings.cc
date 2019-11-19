// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the cast browser to child processes.

#include "chromecast/browser/cast_content_browser_client.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chromecast/browser/application_media_info_manager.h"
#include "chromecast/browser/cast_browser_main_parts.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/media/cdm/cast_cdm_factory.h"
#include "components/network_hints/browser/simple_network_hints_handler_impl.h"
#include "content/public/browser/render_process_host.h"
#include "media/mojo/buildflags.h"

#if BUILDFLAG(ENABLE_CAST_RENDERER)
#include "chromecast/media/service/cast_mojo_media_client.h"
#include "chromecast/media/service/video_geometry_setter_service.h"
#include "media/mojo/mojom/constants.mojom.h"   // nogncheck
#include "media/mojo/services/media_service.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(ENABLE_EXTERNAL_MOJO_SERVICES)
#include "chromecast/external_mojo/broker_service/broker_service.h"
#endif

#if BUILDFLAG(ENABLE_CAST_WAYLAND_SERVER)
#include "chromecast/browser/webview/js_channel_service.h"
#include "chromecast/common/mojom/js_channel.mojom.h"
#endif

#if defined(OS_ANDROID)
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "url/origin.h"
#else
#include "chromecast/browser/memory_pressure_controller_impl.h"
#endif  // defined(OS_ANDROID)

namespace chromecast {
namespace shell {

namespace {

#if defined(OS_ANDROID)
void CreateOriginId(cdm::MediaDrmStorageImpl::OriginIdObtainedCB callback) {
  // TODO(crbug.com/917527): Update this to actually get a pre-provisioned
  // origin ID.
  std::move(callback).Run(true, base::UnguessableToken::Create());
}

void AllowEmptyOriginIdCB(base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

void CreateMediaDrmStorage(content::RenderFrameHost* render_frame_host,
                           ::media::mojom::MediaDrmStorageRequest request) {
  DVLOG(1) << __func__;
  PrefService* pref_service = CastBrowserProcess::GetInstance()->pref_service();
  DCHECK(pref_service);

  if (render_frame_host->GetLastCommittedOrigin().opaque()) {
    DVLOG(1) << __func__ << ": Unique origin.";
    return;
  }

  // The object will be deleted on connection error, or when the frame navigates
  // away.
  new cdm::MediaDrmStorageImpl(
      render_frame_host, pref_service, base::BindRepeating(&CreateOriginId),
      base::BindRepeating(&AllowEmptyOriginIdCB), std::move(request));
}
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_EXTERNAL_MOJO_SERVICES)
void StartExternalMojoBrokerService(
    service_manager::mojom::ServiceRequest request) {
  service_manager::Service::RunAsyncUntilTermination(
      std::make_unique<external_mojo::BrokerService>(std::move(request)));
}
#endif  // BUILDFLAG(ENABLE_EXTERNAL_MOJO_SERVICES)

}  // namespace

void CastContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  registry->AddInterface(
      base::Bind(&media::MediaCapsImpl::AddReceiver,
                 base::Unretained(cast_browser_main_parts_->media_caps())),
      base::ThreadTaskRunnerHandle::Get());

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  if (!memory_pressure_controller_) {
    memory_pressure_controller_.reset(new MemoryPressureControllerImpl());
  }

  registry->AddInterface(
      base::Bind(&MemoryPressureControllerImpl::AddReceiver,
                 base::Unretained(memory_pressure_controller_.get())),
      base::ThreadTaskRunnerHandle::Get());
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
}

void CastContentBrowserClient::ExposeInterfacesToMediaService(
    service_manager::BinderRegistry* registry,
    content::RenderFrameHost* render_frame_host) {
#if defined(OS_ANDROID)
  registry->AddInterface(
      base::BindRepeating(&CreateMediaDrmStorage, render_frame_host));
#endif  // defined(OS_ANDROID)

  std::string application_session_id;
  bool mixer_audio_enabled;
  GetApplicationMediaInfo(&application_session_id, &mixer_audio_enabled,
                          render_frame_host);
  registry->AddInterface(base::BindRepeating(
      &media::CreateApplicationMediaInfoManager, render_frame_host,
      std::move(application_session_id), mixer_audio_enabled));
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void CastContentBrowserClient::CreateMediaService(
    service_manager::mojom::ServiceRequest request) {
  std::unique_ptr<::media::MediaService> service;
  DCHECK(GetMediaTaskRunner() &&
         GetMediaTaskRunner()->BelongsToCurrentThread());
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterServiceOnMediaThread();
  }
  auto mojo_media_client = std::make_unique<media::CastMojoMediaClient>(
      GetCmaBackendFactory(),
      base::Bind(&CastContentBrowserClient::CreateCdmFactory,
                 base::Unretained(this)),
      GetVideoModeSwitcher(), GetVideoResolutionPolicy());
  mojo_media_client->SetVideoGeometrySetterService(
      video_geometry_setter_service_.get());
  service = std::make_unique<::media::MediaService>(
      std::move(mojo_media_client), std::move(request));
  service_manager::Service::RunAsyncUntilTermination(std::move(service));
}

void CastContentBrowserClient::CreateVideoGeometrySetterServiceOnMediaThread() {
  DCHECK(GetMediaTaskRunner() &&
         GetMediaTaskRunner()->BelongsToCurrentThread());
  DCHECK(!video_geometry_setter_service_);
  video_geometry_setter_service_ =
      std::unique_ptr<media::VideoGeometrySetterService,
                      base::OnTaskRunnerDeleter>(
          new media::VideoGeometrySetterService,
          base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));
}

void CastContentBrowserClient::BindVideoGeometrySetterServiceOnMediaThread(
    mojo::GenericPendingReceiver receiver) {
  DCHECK(GetMediaTaskRunner() &&
         GetMediaTaskRunner()->BelongsToCurrentThread());
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterServiceOnMediaThread();
  }
  if (auto r = receiver.As<media::mojom::VideoGeometrySetter>()) {
    video_geometry_setter_service_->GetVideoGeometrySetter(std::move(r));
  }
}

void CastContentBrowserClient::BindGpuHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  DCHECK(GetMediaTaskRunner());
  GetMediaTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&CastContentBrowserClient::
                                    BindVideoGeometrySetterServiceOnMediaThread,
                                base::Unretained(this), std::move(receiver)));
}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

void CastContentBrowserClient::RunServiceInstance(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service>* receiver) {
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  if (identity.name() == ::media::mojom::kMediaRendererServiceName) {
    service_manager::mojom::ServiceRequest request(std::move(*receiver));
    GetMediaTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&CastContentBrowserClient::CreateMediaService,
                                  base::Unretained(this), std::move(request)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(ENABLE_EXTERNAL_MOJO_SERVICES)
  if (identity.name() == external_mojo::BrokerService::kServiceName) {
    StartExternalMojoBrokerService(std::move(*receiver));
    return;
  }
#endif  // BUILDFLAG(ENABLE_EXTERNAL_MOJO_SERVICES)
}

void CastContentBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
#if BUILDFLAG(ENABLE_CAST_WAYLAND_SERVER)
  if (auto r = receiver.As<::chromecast::mojom::JsChannelBindingProvider>()) {
    JsChannelService::Create(render_process_host, std::move(r),
                             base::ThreadTaskRunnerHandle::Get());
    return;
  }
#endif
  if (auto r = receiver.As<::network_hints::mojom::NetworkHintsHandler>()) {
    network_hints::SimpleNetworkHintsHandlerImpl::Create(
        render_process_host->GetID(), std::move(r));
  }
  ContentBrowserClient::BindHostReceiverForRenderer(render_process_host,
                                                    std::move(receiver));
}

}  // namespace shell
}  // namespace chromecast
