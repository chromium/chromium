// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_interface_binders.h"

#include "base/functional/bind.h"
#include "chromecast/browser/application_media_capabilities.h"
#include "chromecast/browser/application_media_info_manager.h"
#include "chromecast/browser/audio_socket_broker.h"
#include "chromecast/browser/cast_navigation_ui_data.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/common/mojom/activity_window.mojom.h"
#include "chromecast/common/mojom/application_media_capabilities.mojom.h"
#include "chromecast/common/mojom/assistant_messenger.mojom.h"
#include "chromecast/common/mojom/audio_socket.mojom.h"
#include "chromecast/common/mojom/cast_demo.mojom.h"
#include "chromecast/common/mojom/gesture.mojom.h"
#include "chromecast/common/mojom/settings_ui.mojom.h"
#include "components/network_hints/browser/simple_network_hints_handler_impl.h"
#include "components/network_hints/common/network_hints.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/mojo/mojom/cast_application_media_info_manager.mojom.h"
#include "media/mojo/mojom/remoting.mojom.h"

namespace chromecast {
namespace shell {

namespace {

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  network_hints::SimpleNetworkHintsHandlerImpl::Create(frame_host,
                                                       std::move(receiver));
}

template <typename Interface>
void BindFromCastWebContents(content::RenderFrameHost* frame_host,
                             mojo::PendingReceiver<Interface> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;
  auto* cast_web_contents = CastWebContents::FromWebContents(web_contents);
  if (!cast_web_contents)
    return;
  mojo::GenericPendingReceiver generic_receiver(std::move(receiver));
  cast_web_contents->TryBindReceiver(generic_receiver);
}

void BindApplicationMediaInfoManager(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<::media::mojom::CastApplicationMediaInfoManager>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    return;
  }
  auto application_session_id =
      CastNavigationUIData::GetSessionIdForWebContents(web_contents);
  media::ApplicationMediaInfoManager::Create(
      frame_host, std::move(application_session_id),
      /*mixer_audio_enabled=*/false, std::move(receiver));
}

void BindAudioSocketBroker(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<::chromecast::mojom::AudioSocketBroker> receiver) {
  media::AudioSocketBroker::Create(frame_host, std::move(receiver));
}

// Some Cast internals still dynamically set up interface binders after
// frame host initialization. This is used to generically forward incoming
// interface receivers to those objects until they can be reworked as static
// registrations below.
bool HandleGenericReceiver(content::RenderFrameHost* frame_host,
                           mojo::GenericPendingReceiver& receiver) {
  // All of the following error scenarios will intentionally crash the render
  // process as a security precaution. On Cast, the errors below are generally
  // safe since the interface endpoints don't expose sensitive functionality.
  // TODO(b/185843831): Just return true in all cases to prevent the crash.
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    LOG(ERROR) << "Could not find target WebContents for receiver.";
    return false;
  }

  // Only WebContents created for Cast Webviews will have a CastWebContents
  // object associated with them. We ignore these requests for any other
  // WebContents.
  auto* cast_web_contents = CastWebContents::FromWebContents(web_contents);
  if (!cast_web_contents) {
    LOG(ERROR) << "Could not find target CastWebContents for receiver.";
    return false;
  }

  if (!cast_web_contents->TryBindReceiver(receiver)) {
    LOG(ERROR) << "Attempt to bind receiver to CastWebContents failed.";
    return false;
  }

  return true;
}

}  // namespace

void PopulateCastFrameBinders(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map) {
  binder_map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));
  binder_map->Add<::media::mojom::CastApplicationMediaInfoManager>(
      base::BindRepeating(&BindApplicationMediaInfoManager));
  binder_map->Add<::chromecast::mojom::AudioSocketBroker>(
      base::BindRepeating(&BindAudioSocketBroker));

  binder_map->Add<mojom::ApplicationMediaCapabilities>(base::BindRepeating(
      &BindFromCastWebContents<mojom::ApplicationMediaCapabilities>));
  binder_map->Add<::media::mojom::Remotee>(
      base::BindRepeating(&BindFromCastWebContents<::media::mojom::Remotee>));
  binder_map->Add<::chromecast::mojom::ActivityWindow>(base::BindRepeating(
      &BindFromCastWebContents<::chromecast::mojom::ActivityWindow>));
  binder_map->Add<
      ::chromecast::mojom::AssistantMessageService>(base::BindRepeating(
      &BindFromCastWebContents<::chromecast::mojom::AssistantMessageService>));
  binder_map->Add<::chromecast::mojom::GestureSource>(base::BindRepeating(
      &BindFromCastWebContents<::chromecast::mojom::GestureSource>));
  binder_map->Add<::chromecast::mojom::SettingsPlatform>(base::BindRepeating(
      &BindFromCastWebContents<::chromecast::mojom::SettingsPlatform>));
  binder_map->Add<mojom::CastDemo>(
      base::BindRepeating(&BindFromCastWebContents<mojom::CastDemo>));

  binder_map->SetDefaultBinderDeprecated(
      base::BindRepeating(&HandleGenericReceiver));
}

}  // namespace shell
}  // namespace chromecast
