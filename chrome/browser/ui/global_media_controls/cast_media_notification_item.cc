// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"

#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_metrics.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"

using Metadata = media_message_center::MediaNotificationViewImpl::Metadata;

namespace {

net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation(
      "media_router_global_media_controls_image",
      R"(
  semantics {
    sender: "Media Router"
    description:
      "Chrome allows users to control media playback on Chromecast-enabled "
      "devices on the same local network. When a media app is running on a "
      "device, it may provide Chrome with metadata including media artwork. "
      "Chrome fetches the artwork so that it can be displayed in the media "
      "controls UI."
    trigger:
      "This is triggered whenever a Cast app running on a device on the local "
      "network sends out a metadata update with a new image URL, e.g. when "
      "the app starts playing a new song or a video."
    data:
      "None, aside from the artwork image URLs specified by Cast apps."
    destination: WEBSITE
  }
  policy {
    cookies_allowed: NO
    setting:
      "The feature is enabled by default. There is no user setting to disable "
      "the feature."
    chrome_policy: {
      EnableMediaRouter {
        EnableMediaRouter: false
      }
    }
  }
)");
}

media_session::mojom::MediaSessionInfoPtr CreateSessionInfo() {
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->state =
      media_session::mojom::MediaSessionInfo::SessionState::kSuspended;
  session_info->force_duck = false;
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  session_info->is_controllable = true;
  session_info->prefer_stop_for_gain_focus_loss = false;
  return session_info;
}

std::vector<media_session::mojom::MediaSessionAction> ToMediaSessionActions(
    const media_router::mojom::MediaStatus& status) {
  std::vector<media_session::mojom::MediaSessionAction> actions;
  // |can_mute| and |can_set_volume| have no MediaSessionAction equivalents.
  if (status.can_play_pause) {
    actions.push_back(media_session::mojom::MediaSessionAction::kPlay);
    actions.push_back(media_session::mojom::MediaSessionAction::kPause);
  }
  if (status.can_seek) {
    actions.push_back(media_session::mojom::MediaSessionAction::kSeekBackward);
    actions.push_back(media_session::mojom::MediaSessionAction::kSeekForward);
    actions.push_back(media_session::mojom::MediaSessionAction::kSeekTo);
  }
  if (status.can_skip_to_next_track) {
    actions.push_back(media_session::mojom::MediaSessionAction::kNextTrack);
  }
  if (status.can_skip_to_previous_track) {
    actions.push_back(media_session::mojom::MediaSessionAction::kPreviousTrack);
  }
  return actions;
}

media_session::mojom::MediaPlaybackState ToPlaybackState(
    media_router::mojom::MediaStatus::PlayState play_state) {
  switch (play_state) {
    case media_router::mojom::MediaStatus::PlayState::PLAYING:
      return media_session::mojom::MediaPlaybackState::kPlaying;
    case media_router::mojom::MediaStatus::PlayState::PAUSED:
      return media_session::mojom::MediaPlaybackState::kPaused;
    case media_router::mojom::MediaStatus::PlayState::BUFFERING:
      return media_session::mojom::MediaPlaybackState::kPlaying;
  }
}

media_session::mojom::MediaSessionInfo::SessionState ToSessionState(
    media_router::mojom::MediaStatus::PlayState play_state) {
  switch (play_state) {
    case media_router::mojom::MediaStatus::PlayState::PLAYING:
      return media_session::mojom::MediaSessionInfo::SessionState::kActive;
    case media_router::mojom::MediaStatus::PlayState::PAUSED:
      return media_session::mojom::MediaSessionInfo::SessionState::kSuspended;
    case media_router::mojom::MediaStatus::PlayState::BUFFERING:
      return media_session::mojom::MediaSessionInfo::SessionState::kActive;
  }
}

std::u16string GetSourceTitle(const media_router::MediaRoute& route) {
#if !BUILDFLAG(IS_CHROMEOS)
  // Never include the media sink name for updated media UI on non-CrOS.
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsUpdatedUI)) {
    if (route.description().empty()) {
      return l10n_util::GetStringUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_UNKNOWN_SOURCE_TEXT);
    }
    return base::UTF8ToUTF16(route.description());
  }
#endif

  if (route.media_sink_name().empty())
    return base::UTF8ToUTF16(route.description());

  if (route.description().empty())
    return base::UTF8ToUTF16(route.media_sink_name());

  const char kSeparator[] = " \xC2\xB7 ";  // "Middle dot" character.
  const std::string source_title =
      base::i18n::IsRTL()
          ? route.media_sink_name() + kSeparator + route.description()
          : route.description() + kSeparator + route.media_sink_name();
  return base::UTF8ToUTF16(source_title);
}

}  // namespace

CastMediaNotificationItem::CastMediaNotificationItem(
    const media_router::MediaRoute& route,
    global_media_controls::MediaItemManager* item_manager,
    std::unique_ptr<CastMediaSessionController> session_controller,
    Profile* profile)
    : item_manager_(item_manager),
      profile_(profile),
      session_controller_(std::move(session_controller)),
      media_route_id_(route.media_route_id()),
      route_is_local_(route.is_local()),
      image_downloader_(
          profile,
          base::BindRepeating(&CastMediaNotificationItem::ImageChanged,
                              base::Unretained(this))),
      session_info_(CreateSessionInfo()) {
  metadata_.source_title = GetSourceTitle(route);
  device_name_ = route.media_sink_name();
}

CastMediaNotificationItem::~CastMediaNotificationItem() {
  item_manager_->HideItem(media_route_id_);
}

void CastMediaNotificationItem::SetView(
    media_message_center::MediaNotificationView* view) {
  view_ = view;
  if (view_)
    view_->UpdateWithVectorIcon(&vector_icons::kMediaRouterIdleIcon);

  UpdateView();
}

void CastMediaNotificationItem::OnMediaSessionActionButtonPressed(
    media_session::mojom::MediaSessionAction action) {
  base::UmaHistogramEnumeration(kUserActionHistogramName, action);
  base::UmaHistogramEnumeration(kCastUserActionHistogramName, action);
  session_controller_->Send(action);
}

void CastMediaNotificationItem::SeekTo(base::TimeDelta time) {
  session_controller_->SeekTo(time);
}

void CastMediaNotificationItem::Dismiss() {
  item_manager_->HideItem(media_route_id_);
  is_active_ = false;
}

void CastMediaNotificationItem::SetVolume(float volume) {
  session_controller_->SetVolume(volume);
}
void CastMediaNotificationItem::SetMute(bool mute) {
  session_controller_->SetMute(mute);
}

bool CastMediaNotificationItem::RequestMediaRemoting() {
  return false;
}

media_message_center::Source CastMediaNotificationItem::GetSource() const {
  return route_is_local_ ? media_message_center::Source::kLocalCastSession
                         : media_message_center::Source::kNonLocalCastSession;
}

media_message_center::SourceType CastMediaNotificationItem::GetSourceType()
    const {
  return media_message_center::SourceType::kCast;
}

std::optional<base::UnguessableToken> CastMediaNotificationItem::GetSourceId()
    const {
  return std::nullopt;
}

void CastMediaNotificationItem::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr status) {
  metadata_.title = base::UTF8ToUTF16(status->title);
  metadata_.artist = base::UTF8ToUTF16(status->secondary_title);
  actions_ = ToMediaSessionActions(*status);
  session_info_->state = ToSessionState(status->play_state);
  session_info_->playback_state = ToPlaybackState(status->play_state);
  is_muted_ = status->is_muted;
  volume_ = status->volume;

  // Make sure |current_time| is always less than or equal to |duration|
  base::TimeDelta duration = status->duration;
  base::TimeDelta current_time =
      status->current_time > duration ? duration : status->current_time;
  constexpr bool kUnused = false;
  media_position_ = media_session::MediaPosition(
      /*playback_rate=*/status->play_state ==
              media_router::mojom::MediaStatus::PlayState::PLAYING
          ? 1.0
          : 0.0,
      duration, current_time, /*end_of_media=*/kUnused);

  if (status->images.empty()) {
    image_downloader_.Reset();
  } else {
    // TODO(takumif): Consider choosing an image based on the resolution.
    image_downloader_.Download(status->images.at(0)->url);
  }
  UpdateView();
  session_controller_->OnMediaStatusUpdated(std::move(status));
}

void CastMediaNotificationItem::OnRouteUpdated(
    const media_router::MediaRoute& route) {
  CHECK_EQ(route.media_route_id(), media_route_id_);
  device_name_ = route.media_sink_name();

  bool updated = false;
  const std::u16string new_source_title = GetSourceTitle(route);
  if (metadata_.source_title != new_source_title) {
    metadata_.source_title = new_source_title;
    updated = true;
  }
  const std::u16string new_artist = base::UTF8ToUTF16(route.description());
  if (metadata_.artist != new_artist) {
    metadata_.artist = new_artist;
    updated = true;
  }
  if (updated && view_)
    view_->UpdateWithMediaMetadata(metadata_);
}

void CastMediaNotificationItem::StopCasting() {
  media_router::MediaRouterFactory::GetApiForBrowserContext(profile_)
      ->TerminateRoute(media_route_id_);

  item_manager_->FocusDialog();

  feature_engagement::TrackerFactory::GetForBrowserContext(profile_)
      ->NotifyEvent("media_route_stopped_from_gmc");

  MediaItemUIMetrics::RecordStopCastingMetrics(
      media_router::MediaCastMode::PRESENTATION);
}

mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
CastMediaNotificationItem::GetObserverPendingRemote() {
  return observer_receiver_.BindNewPipeAndPassRemote();
}

CastMediaNotificationItem::ImageDownloader::ImageDownloader(
    Profile* profile,
    base::RepeatingCallback<void(const SkBitmap&)> callback)
    : url_loader_factory_(profile->GetDefaultStoragePartition()
                              ->GetURLLoaderFactoryForBrowserProcess()),
      callback_(std::move(callback)) {}

CastMediaNotificationItem::ImageDownloader::~ImageDownloader() = default;

void CastMediaNotificationItem::ImageDownloader::OnFetchComplete(
    const GURL& url,
    const SkBitmap* bitmap) {
  if (bitmap) {
    bitmap_ = *bitmap;
    callback_.Run(*bitmap);
  }
}

void CastMediaNotificationItem::ImageDownloader::Download(const GURL& url) {
  if (url == url_)
    return;
  url_ = url;
  bitmap_fetcher_ = bitmap_fetcher_factory_for_testing_
                        ? bitmap_fetcher_factory_for_testing_.Run(
                              url_, this, GetTrafficAnnotationTag())
                        : std::make_unique<BitmapFetcher>(
                              url_, this, GetTrafficAnnotationTag());
  bitmap_fetcher_->Init(net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
  bitmap_fetcher_->Start(url_loader_factory_.get());
}

void CastMediaNotificationItem::ImageDownloader::Reset() {
  bitmap_fetcher_.reset();
  url_ = GURL();
  bitmap_ = SkBitmap();
}

void CastMediaNotificationItem::UpdateView() {
  if (!view_)
    return;

  view_->UpdateWithMediaMetadata(metadata_);
  view_->UpdateWithMediaActions(actions_);
  view_->UpdateWithMediaSessionInfo(session_info_.Clone());
  view_->UpdateWithMediaArtwork(
      gfx::ImageSkia::CreateFrom1xBitmap(image_downloader_.bitmap()));
  if (!media_position_.duration().is_zero())
    view_->UpdateWithMediaPosition(media_position_);
  view_->UpdateWithMuteStatus(is_muted_);
  view_->UpdateWithVolume(volume_);
}

void CastMediaNotificationItem::ImageChanged(const SkBitmap& bitmap) {
  if (view_)
    view_->UpdateWithMediaArtwork(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}
