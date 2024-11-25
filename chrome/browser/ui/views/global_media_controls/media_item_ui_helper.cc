// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_metrics.h"
#include "chrome/browser/ui/views/global_media_controls/cast_device_footer_view.h"
#include "chrome/browser/ui/views/global_media_controls/cast_device_selector_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_cast_footer_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_legacy_cast_footer_view.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_session_notification_item.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/media_session.h"
#include "media/base/media_switches.h"
#include "ui/color/color_id.h"

namespace {

void UpdateMediaSessionItemReceiverName(
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    const std::optional<media_router::MediaRoute>& route) {
  if (item->GetSourceType() ==
      media_message_center::SourceType::kLocalMediaSession) {
    auto* media_session_item =
        static_cast<global_media_controls::MediaSessionNotificationItem*>(
            item.get());
    if (route.has_value()) {
      media_session_item->UpdateDeviceName(route->media_sink_name());
    } else {
      media_session_item->UpdateDeviceName(std::nullopt);
    }
  }
}

}  // namespace

HostAndClientPair::HostAndClientPair() = default;
HostAndClientPair::HostAndClientPair(HostAndClientPair&&) = default;
HostAndClientPair& HostAndClientPair::operator=(HostAndClientPair&&) = default;
HostAndClientPair::~HostAndClientPair() = default;

bool ShouldShowDeviceSelectorView(
    Profile* profile,
    global_media_controls::mojom::DeviceService* device_service,
    const std::string& item_id,
    const base::WeakPtr<media_message_center::MediaNotificationItem>& item,

    MediaItemUIDeviceSelectorDelegate* selector_delegate) {
  if (!device_service || !selector_delegate || !profile || !item) {
    return false;
  }

  auto source_type = item->GetSourceType();
  if (source_type == media_message_center::SourceType::kCast) {
    return false;
  }
  if (!media_router::GlobalMediaControlsCastStartStopEnabled(profile) &&
      !base::FeatureList::IsEnabled(
          media::kGlobalMediaControlsSeamlessTransfer)) {
    return false;
  }
  // Hide device selector view if the local media session has started Remote
  // Playback or Tab Mirroring.
  if (source_type == media_message_center::SourceType::kLocalMediaSession &&
      GetSessionRoute(item_id, item, profile).has_value()) {
    return false;
  }
  return true;
}

HostAndClientPair CreateHostAndClient(
    Profile* profile,
    const std::string& id,
    const base::WeakPtr<media_message_center::MediaNotificationItem>& item,
    global_media_controls::mojom::DeviceService* device_service) {
  mojo::PendingRemote<global_media_controls::mojom::DeviceListHost> host;
  mojo::PendingRemote<global_media_controls::mojom::DeviceListClient> client;
  auto client_receiver = client.InitWithNewPipeAndPassReceiver();
  if (media_router::GlobalMediaControlsCastStartStopEnabled(profile)) {
    if (item->GetSourceType() ==
        media_message_center::SourceType::kLocalMediaSession) {
      device_service->GetDeviceListHostForSession(
          id, host.InitWithNewPipeAndPassReceiver(), std::move(client));
    } else {
      device_service->GetDeviceListHostForPresentation(
          host.InitWithNewPipeAndPassReceiver(), std::move(client));
    }
  }
  HostAndClientPair host_and_client;
  host_and_client.host = std::move(host);
  host_and_client.client = std::move(client_receiver);

  return host_and_client;
}

base::RepeatingClosure GetStopCastingCallback(
    Profile* profile,
    const std::string& id,
    const base::WeakPtr<media_message_center::MediaNotificationItem>& item) {
  base::RepeatingClosure stop_casting_callback;

  // Show a footer view for a local media item when it has an associated Remote
  // Playback session or a Tab Mirroring Session.
  if (item->GetSourceType() !=
      media_message_center::SourceType::kLocalMediaSession) {
    return stop_casting_callback;
  }
  auto route = GetSessionRoute(id, item, profile);
  UpdateMediaSessionItemReceiverName(item, route);
  if (!route.has_value()) {
    return stop_casting_callback;
  }

  const auto& route_id = route->media_route_id();
  auto cast_mode = HasRemotePlaybackRoute(item)
                       ? media_router::MediaCastMode::REMOTE_PLAYBACK
                       : media_router::MediaCastMode::TAB_MIRROR;

  stop_casting_callback = base::BindRepeating(
      [](const std::string& route_id, media_router::MediaRouter* router,
         media_router::MediaCastMode cast_mode) {
        router->TerminateRoute(route_id);
        MediaItemUIMetrics::RecordStopCastingMetrics(cast_mode);
        if (cast_mode == media_router::MediaCastMode::TAB_MIRROR) {
          MediaDialogView::HideDialog();
        }
      },
      route_id,
      media_router::MediaRouterFactory::GetApiForBrowserContext(profile),
      cast_mode);
  return stop_casting_callback;
}

bool HasRemotePlaybackRoute(
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (base::FeatureList::IsEnabled(media::kMediaRemotingWithoutFullscreen) &&
      item &&
      item->GetSourceType() ==
          media_message_center::SourceType::kLocalMediaSession) {
    const auto* media_session_item =
        static_cast<global_media_controls::MediaSessionNotificationItem*>(
            item.get());
    return media_session_item->GetRemotePlaybackMetadata() &&
           media_session_item->GetRemotePlaybackMetadata()
               ->remote_playback_started;
  }
  return false;
}

std::optional<media_router::MediaRoute> GetSessionRoute(
    const std::string& item_id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    content::BrowserContext* context) {
  if (!media_router::MediaRouterEnabled(context)) {
    return std::nullopt;
  }

  // Return std::nullopt if the item is not a local media session.
  if (!item || item->GetSourceType() !=
                   media_message_center::SourceType::kLocalMediaSession) {
    return std::nullopt;
  }

  // Return std::nullopt if fallback to tab mirroring is disabled, and
  // media session doesn't have an associated Remote Playback route.
  if (!base::FeatureList::IsEnabled(
          media_router::kFallbackToAudioTabMirroring) &&
      !HasRemotePlaybackRoute(item)) {
    return std::nullopt;
  }

  auto* web_contents =
      content::MediaSession::GetWebContentsFromRequestId(item_id);
  if (!web_contents) {
    return std::nullopt;
  }

  const int item_tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents).id();
  for (const auto& route :
       media_router::MediaRouterFactory::GetApiForBrowserContext(context)
           ->GetCurrentRoutes()) {
    media_router::MediaSource media_source = route.media_source();
    std::optional<int> tab_id_from_route_id;
    if (media_source.IsRemotePlaybackSource()) {
      tab_id_from_route_id = media_source.TabIdFromRemotePlaybackSource();
    } else if (media_source.IsTabMirroringSource()) {
      tab_id_from_route_id = media_source.TabId();
    }

    if (tab_id_from_route_id.has_value() &&
        tab_id_from_route_id.value() == item_tab_id) {
      return route;
    }
  }

  return std::nullopt;
}

std::unique_ptr<global_media_controls::MediaItemUIDeviceSelector>
BuildDeviceSelector(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    global_media_controls::mojom::DeviceService* device_service,
    MediaItemUIDeviceSelectorDelegate* selector_delegate,
    Profile* profile,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    bool show_devices,
    std::optional<media_message_center::MediaColorTheme> media_color_theme) {
  if (!ShouldShowDeviceSelectorView(profile, device_service, id, item,
                                    selector_delegate)) {
    return nullptr;
  }

  auto device_set = CreateHostAndClient(profile, id, item, device_service);

#if !BUILDFLAG(IS_CHROMEOS)
  if (media_color_theme.has_value()) {
    return std::make_unique<CastDeviceSelectorView>(
        std::move(device_set.host), std::move(device_set.client),
        media_color_theme.value(), show_devices);
  }
#endif

  const bool is_local_media_session =
      item->GetSourceType() ==
      media_message_center::SourceType::kLocalMediaSession;

  return std::make_unique<MediaItemUIDeviceSelectorView>(
      id, selector_delegate, std::move(device_set.host),
      std::move(device_set.client),
      /*has_audio_output=*/is_local_media_session, entry_point, show_devices,
      media_color_theme);
}

std::unique_ptr<global_media_controls::MediaItemUIFooter> BuildFooter(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    Profile* profile,
    std::optional<media_message_center::MediaColorTheme> media_color_theme) {
  // Show a footer view for a Cast item.
  if (item->GetSourceType() == media_message_center::SourceType::kCast &&
      media_router::GlobalMediaControlsCastStartStopEnabled(profile)) {
    auto media_cast_item =
        static_cast<CastMediaNotificationItem*>(item.get())->GetWeakPtr();
#if BUILDFLAG(IS_CHROMEOS)
    if (media_color_theme.has_value()) {
      return std::make_unique<MediaItemUICastFooterView>(
          base::BindRepeating(&CastMediaNotificationItem::StopCasting,
                              media_cast_item),
          media_color_theme.value());
    }
#else
    if (media_color_theme.has_value()) {
      return std::make_unique<CastDeviceFooterView>(
          media_cast_item->device_name(),
          base::BindRepeating(&CastMediaNotificationItem::StopCasting,
                              media_cast_item),
          media_color_theme.value());
    }
#endif

    return std::make_unique<MediaItemUILegacyCastFooterView>(
        base::BindRepeating(&CastMediaNotificationItem::StopCasting,
                            media_cast_item));
  }

  base::RepeatingClosure stop_casting_cb =
      GetStopCastingCallback(profile, id, item);
  if (stop_casting_cb.is_null()) {
    return nullptr;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (media_color_theme.has_value()) {
    auto* media_session_item =
        static_cast<global_media_controls::MediaSessionNotificationItem*>(
            item.get());
    return std::make_unique<CastDeviceFooterView>(
        media_session_item->device_name(), std::move(stop_casting_cb),
        media_color_theme.value());
  }
#endif

  return std::make_unique<MediaItemUILegacyCastFooterView>(
      std::move(stop_casting_cb));
}

media_message_center::MediaColorTheme GetMediaColorTheme() {
  media_message_center::MediaColorTheme theme;
  theme.primary_foreground_color_id = ui::kColorSysOnSurface;
  theme.secondary_foreground_color_id = ui::kColorSysOnSurfaceSubtle;

  // Colors for the play/pause button.
  theme.play_button_foreground_color_id = ui::kColorSysOnPrimary;
  theme.play_button_container_color_id = ui::kColorSysPrimary;
  theme.pause_button_foreground_color_id = ui::kColorSysOnTonalContainer;
  theme.pause_button_container_color_id = ui::kColorSysTonalContainer;

  // Colors for the progress view.
  theme.playing_progress_foreground_color_id = ui::kColorSysPrimary;
  theme.playing_progress_background_color_id =
      ui::kColorSysStateDisabledContainer;
  theme.paused_progress_foreground_color_id =
      ui::kColorSysStateDisabledContainer;
  theme.paused_progress_background_color_id =
      ui::kColorSysStateDisabledContainer;

  theme.background_color_id = ui::kColorSysSurface2;
  theme.device_selector_border_color_id = ui::kColorSysDivider;
  theme.device_selector_foreground_color_id = ui::kColorSysPrimary;
  theme.device_selector_background_color_id = ui::kColorSysSurface5;
  theme.error_foreground_color_id = ui::kColorSysError;
  theme.error_container_color_id = ui::kColorSysErrorContainer;
  theme.focus_ring_color_id = ui::kColorSysStateFocusRing;
  return theme;
}

const gfx::VectorIcon& GetVectorIcon(
    global_media_controls::mojom::IconType icon) {
  switch (icon) {
    case global_media_controls::mojom::IconType::kInfo:
      return kInfoIcon;
    case global_media_controls::mojom::IconType::kSpeaker:
      return kSpeakerIcon;
    case global_media_controls::mojom::IconType::kSpeakerGroup:
      return kSpeakerGroupIcon;
    case global_media_controls::mojom::IconType::kInput:
      return kInputIcon;
    case global_media_controls::mojom::IconType::kThrobber:
    case global_media_controls::mojom::IconType::kTv:
    case global_media_controls::mojom::IconType::kUnknown:
      return kTvIcon;
  }
}
