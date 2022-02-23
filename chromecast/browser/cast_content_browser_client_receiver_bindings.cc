// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the cast browser to child processes.

#include "chromecast/browser/cast_content_browser_client.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chromecast/browser/application_media_info_manager.h"
#include "chromecast/browser/cast_browser_interface_binders.h"
#include "chromecast/browser/cast_browser_main_parts.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/browser/metrics/metrics_helper_impl.h"
#include "chromecast/browser/service_connector.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/media/cdm/cast_cdm_factory.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "content/public/browser/render_process_host.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_CAST_RENDERER)
#include "chromecast/media/service/cast_mojo_media_client.h"
#include "chromecast/media/service/video_geometry_setter_service.h"
#include "media/mojo/services/media_service.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_OZONE)
#include "chromecast/browser/webview/js_channel_service.h"
#include "chromecast/common/mojom/js_channel.mojom.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chromecast/browser/memory_pressure_controller_impl.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/guest_view/extensions_guest_view.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#endif

namespace chromecast {
namespace shell {

namespace {

void CreateOriginId(cdm::MediaDrmStorageImpl::OriginIdObtainedCB callback) {
  std::move(callback).Run(true, base::UnguessableToken::Create());
}

void AllowEmptyOriginIdCB(base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

void CreateMediaDrmStorage(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<::media::mojom::MediaDrmStorage> receiver) {
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
      base::BindRepeating(&AllowEmptyOriginIdCB), std::move(receiver));
}

}  // namespace

void CastContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  registry->AddInterface(
      base::BindRepeating(
          &media::MediaCapsImpl::AddReceiver,
          base::Unretained(cast_browser_main_parts_->media_caps())),
      base::ThreadTaskRunnerHandle::Get());

  registry->AddInterface(
      base::BindRepeating(
          &metrics::MetricsHelperImpl::AddReceiver,
          base::Unretained(cast_browser_main_parts_->metrics_helper())),
      base::ThreadTaskRunnerHandle::Get());

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  if (!memory_pressure_controller_) {
    memory_pressure_controller_.reset(new MemoryPressureControllerImpl());
  }

  registry->AddInterface(
      base::BindRepeating(&MemoryPressureControllerImpl::AddReceiver,
                          base::Unretained(memory_pressure_controller_.get())),
      base::ThreadTaskRunnerHandle::Get());
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  associated_registry->AddInterface(base::BindRepeating(
      &extensions::EventRouter::BindForRenderer, render_process_host->GetID()));
  associated_registry->AddInterface(
      base::BindRepeating(&extensions::ExtensionsGuestView::CreateForComponents,
                          render_process_host->GetID()));
  associated_registry->AddInterface(
      base::BindRepeating(&extensions::ExtensionsGuestView::CreateForExtensions,
                          render_process_host->GetID()));
#endif
}

void CastContentBrowserClient::BindMediaServiceReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<::media::mojom::MediaDrmStorage>()) {
    CreateMediaDrmStorage(render_frame_host, std::move(r));
    return;
  }

  if (auto r = receiver.As<mojom::ServiceConnector>()) {
    ServiceConnector::BindReceiver(kMediaServiceClientId, std::move(r));
    return;
  }

  if (auto r = receiver.As<::media::mojom::CastApplicationMediaInfoManager>()) {
    std::string application_session_id;
    bool mixer_audio_enabled;
    GetApplicationMediaInfo(&application_session_id, &mixer_audio_enabled,
                            render_frame_host);
    media::CreateApplicationMediaInfoManager(render_frame_host,
                                             std::move(application_session_id),
                                             mixer_audio_enabled, std::move(r));
    return;
  }
}

void CastContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  PopulateCastFrameBinders(render_frame_host, map);
}

mojo::Remote<::media::mojom::MediaService>
CastContentBrowserClient::RunSecondaryMediaService() {
  mojo::Remote<::media::mojom::MediaService> remote;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  GetMediaTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&CastContentBrowserClient::CreateMediaService,
                                base::Unretained(this),
                                remote.BindNewPipeAndPassReceiver()));
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)
  return remote;
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void CastContentBrowserClient::CreateMediaService(
    mojo::PendingReceiver<::media::mojom::MediaService> receiver) {
  DCHECK(GetMediaTaskRunner() &&
         GetMediaTaskRunner()->BelongsToCurrentThread());
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterServiceOnMediaThread();
  }

  // Using base::Unretained is safe here because this class will persist for
  // the duration of the browser process' lifetime.
  auto mojo_media_client = std::make_unique<media::CastMojoMediaClient>(
      GetCmaBackendFactory(),
      base::BindRepeating(&CastContentBrowserClient::CreateCdmFactory,
                          base::Unretained(this)),
      GetVideoModeSwitcher(), GetVideoResolutionPolicy(),
      browser_main_parts()->media_connector(),
      base::BindRepeating(&CastContentBrowserClient::IsBufferingEnabled,
                          base::Unretained(this)));
  mojo_media_client->SetVideoGeometrySetterService(
      video_geometry_setter_service_.get());

  static base::SequenceLocalStorageSlot<::media::MediaService> service;
  service.emplace(std::move(mojo_media_client), std::move(receiver));
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
    mojo::PendingReceiver<service_manager::mojom::Service>* receiver) {}

void CastContentBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_OZONE)
  if (auto r = receiver.As<::chromecast::mojom::JsChannelBindingProvider>()) {
    JsChannelService::Create(render_process_host, std::move(r),
                             base::ThreadTaskRunnerHandle::Get());
    return;
  }
#endif
  ContentBrowserClient::BindHostReceiverForRenderer(render_process_host,
                                                    std::move(receiver));
}

}  // namespace shell
}  // namespace chromecast
