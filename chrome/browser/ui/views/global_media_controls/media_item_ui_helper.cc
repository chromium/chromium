// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"

#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_metrics.h"
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

namespace {

bool ShouldShowDeviceSelectorView(
    const std::string& item_id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    Profile* profile) {
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

void UpdateMediaSessionItemReceiverName(
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    const absl::optional<media_router::MediaRoute>& route) {
  if (item->GetSourceType() ==
      media_message_center::SourceType::kLocalMediaSession) {
    auto* media_session_item =
        static_cast<global_media_controls::MediaSessionNotificationItem*>(
            item.get());
    if (route.has_value()) {
      media_session_item->UpdateDeviceName(route->media_sink_name());
    } else {
      media_session_item->UpdateDeviceName(absl::nullopt);
    }
  }
}

}  // namespace

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

absl::optional<media_router::MediaRoute> GetSessionRoute(
    const std::string& item_id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    content::BrowserContext* context) {
  if (!media_router::MediaRouterEnabled(context)) {
    return absl::nullopt;
  }

  // Return absl::nullopt if the item is not a local media session.
  if (!item || item->GetSourceType() !=
                   media_message_center::SourceType::kLocalMediaSession) {
    return absl::nullopt;
  }

  // Return absl::nullopt if fallback to tab mirroring is disabled, and
  // media session doesn't have an associated Remote Playback route.
  if (!base::FeatureList::IsEnabled(
          media_router::kFallbackToAudioTabMirroring) &&
      !HasRemotePlaybackRoute(item)) {
    return absl::nullopt;
  }

  auto* web_contents =
      content::MediaSession::GetWebContentsFromRequestId(item_id);
  if (!web_contents) {
    return absl::nullopt;
  }

  const int item_tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents).id();
  for (const auto& route :
       media_router::MediaRouterFactory::GetApiForBrowserContext(context)
           ->GetCurrentRoutes()) {
    media_router::MediaSource media_source = route.media_source();
    absl::optional<int> tab_id_from_route_id;
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

  return absl::nullopt;
}

std::unique_ptr<MediaItemUIDeviceSelectorView> BuildDeviceSelector(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    global_media_controls::mojom::DeviceService* device_service,
    MediaItemUIDeviceSelectorDelegate* selector_delegate,
    Profile* profile,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    bool show_devices,
    absl::optional<media_message_center::MediaColorTheme> media_color_theme) {
  if (!device_service || !selector_delegate || !profile ||
      !ShouldShowDeviceSelectorView(id, item, profile)) {
    return nullptr;
  }

  const bool is_local_media_session =
      item->GetSourceType() ==
      media_message_center::SourceType::kLocalMediaSession;
  const bool gmc_cast_start_stop_enabled =
      media_router::GlobalMediaControlsCastStartStopEnabled(profile);
  const bool show_expand_button =
      !base::FeatureList::IsEnabled(media::kGlobalMediaControlsModernUI);
  mojo::PendingRemote<global_media_controls::mojom::DeviceListHost> host;
  mojo::PendingRemote<global_media_controls::mojom::DeviceListClient> client;
  auto client_receiver = client.InitWithNewPipeAndPassReceiver();
  if (gmc_cast_start_stop_enabled) {
    if (is_local_media_session) {
      device_service->GetDeviceListHostForSession(
          id, host.InitWithNewPipeAndPassReceiver(), std::move(client));
    } else {
      device_service->GetDeviceListHostForPresentation(
          host.InitWithNewPipeAndPassReceiver(), std::move(client));
    }
  }
  return std::make_unique<MediaItemUIDeviceSelectorView>(
      id, selector_delegate, std::move(host), std::move(client_receiver),
      /* has_audio_output */ is_local_media_session, entry_point,
      show_expand_button, show_devices, media_color_theme);
}

std::unique_ptr<global_media_controls::MediaItemUIFooter> BuildFooter(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    Profile* profile,
    absl::optional<media_message_center::MediaColorTheme> media_color_theme) {
  // Show a footer view for a Cast item.
  if (item->GetSourceType() == media_message_center::SourceType::kCast &&
      media_router::GlobalMediaControlsCastStartStopEnabled(profile)) {
#if BUILDFLAG(IS_CHROMEOS)
    if (base::FeatureList::IsEnabled(
            media::kGlobalMediaControlsCrOSUpdatedUI) &&
        media_color_theme.has_value()) {
      return std::make_unique<MediaItemUICastFooterView>(
          base::BindRepeating(
              &CastMediaNotificationItem::StopCasting,
              static_cast<CastMediaNotificationItem*>(item.get())
                  ->GetWeakPtr()),
          media_color_theme.value());
    }
#endif
    return std::make_unique<MediaItemUILegacyCastFooterView>(
        base::BindRepeating(
            &CastMediaNotificationItem::StopCasting,
            static_cast<CastMediaNotificationItem*>(item.get())->GetWeakPtr()));
  }

  // Show a footer view for a local media item when it has an associated Remote
  // Playback session or a Tab Mirroring Session.
  if (item->GetSourceType() !=
      media_message_center::SourceType::kLocalMediaSession) {
    return nullptr;
  }

  auto route = GetSessionRoute(id, item, profile);
  UpdateMediaSessionItemReceiverName(item, route);
  if (!route.has_value()) {
    return nullptr;
  }

  const auto& route_id = route->media_route_id();
  auto cast_mode = HasRemotePlaybackRoute(item)
                       ? media_router::MediaCastMode::REMOTE_PLAYBACK
                       : media_router::MediaCastMode::TAB_MIRROR;

  auto stop_casting_cb = base::BindRepeating(
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
  return std::make_unique<MediaItemUILegacyCastFooterView>(
      std::move(stop_casting_cb));
}
