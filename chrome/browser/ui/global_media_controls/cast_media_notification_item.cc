// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"

#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

using Metadata = media_message_center::MediaNotificationViewImpl::Metadata;

namespace {

constexpr char kArtworkHistogramName[] =
    "Media.Notification.Cast.ArtworkPresent";
constexpr char kMetadataHistogramName[] =
    "Media.Notification.Cast.MetadataPresent";

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

base::string16 GetSourceTitle(const media_router::MediaRoute& route) {
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
    media_message_center::MediaNotificationController* notification_controller,
    std::unique_ptr<CastMediaSessionController> session_controller,
    Profile* profile)
    : notification_controller_(notification_controller),
      session_controller_(std::move(session_controller)),
      media_route_id_(route.media_route_id()),
      image_downloader_(
          profile,
          base::BindRepeating(&CastMediaNotificationItem::ImageChanged,
                              base::Unretained(this))),
      session_info_(CreateSessionInfo()) {
  metadata_.source_title = GetSourceTitle(route);
  base::UmaHistogramEnumeration(
      kSourceHistogramName, route.is_local() ? Source::kLocalCastSession
                                             : Source::kNonLocalCastSession);
}

CastMediaNotificationItem::~CastMediaNotificationItem() {
  notification_controller_->HideNotification(media_route_id_);
}

void CastMediaNotificationItem::SetView(
    media_message_center::MediaNotificationView* view) {
  view_ = view;
  if (view_)
    view_->UpdateWithVectorIcon(vector_icons::kMediaRouterIdleIcon);

  UpdateView();
  if (view_ && !recorded_metadata_metrics_) {
    recorded_metadata_metrics_ = true;
    // We record the metadata shown after a delay because if the view is shown
    // as soon as the Cast session is launched, it'd take some time for Chrome
    // to receive status info and fetch the artwork. We need to use a fixed
    // delay rather than waiting for OnMediaStatusUpdated(), because it could
    // get called multiple times with increasing amounts of info, or not get
    // called at all.
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CastMediaNotificationItem::RecordMetadataMetrics,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(3));
  }
}

void CastMediaNotificationItem::OnMediaSessionActionButtonPressed(
    media_session::mojom::MediaSessionAction action) {
  base::UmaHistogramEnumeration(kUserActionHistogramName, action);
  base::UmaHistogramEnumeration(kCastUserActionHistogramName, action);
  session_controller_->Send(action);
}

void CastMediaNotificationItem::Dismiss() {
  notification_controller_->HideNotification(media_route_id_);
}

bool CastMediaNotificationItem::SourceIsCast() {
  return true;
}

void CastMediaNotificationItem::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr status) {
  metadata_.title = base::UTF8ToUTF16(status->title);
  metadata_.artist = base::UTF8ToUTF16(status->secondary_title);
  actions_ = ToMediaSessionActions(*status);
  session_info_->state = ToSessionState(status->play_state);
  session_info_->playback_state = ToPlaybackState(status->play_state);

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
  DCHECK_EQ(route.media_route_id(), media_route_id_);
  bool updated = false;
  const base::string16 new_source_title = GetSourceTitle(route);
  if (metadata_.source_title != new_source_title) {
    metadata_.source_title = new_source_title;
    updated = true;
  }
  const base::string16 new_artist = base::UTF8ToUTF16(route.description());
  if (metadata_.artist != new_artist) {
    metadata_.artist = new_artist;
    updated = true;
  }
  if (updated && view_)
    view_->UpdateWithMediaMetadata(metadata_);
}

mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
CastMediaNotificationItem::GetObserverPendingRemote() {
  return observer_receiver_.BindNewPipeAndPassRemote();
}

CastMediaNotificationItem::ImageDownloader::ImageDownloader(
    Profile* profile,
    base::RepeatingCallback<void(const SkBitmap&)> callback)
    : url_loader_factory_(
          content::BrowserContext::GetDefaultStoragePartition(profile)
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
  bitmap_fetcher_->Init(
      /* referrer */ "", net::ReferrerPolicy::NEVER_CLEAR,
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
}

void CastMediaNotificationItem::ImageChanged(const SkBitmap& bitmap) {
  if (view_)
    view_->UpdateWithMediaArtwork(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

void CastMediaNotificationItem::RecordMetadataMetrics() const {
  base::UmaHistogramBoolean(kArtworkHistogramName,
                            !image_downloader_.bitmap().empty());

  base::UmaHistogramEnumeration(kMetadataHistogramName, Metadata::kCount);
  if (!metadata_.title.empty())
    base::UmaHistogramEnumeration(kMetadataHistogramName, Metadata::kTitle);
  if (!metadata_.artist.empty())
    base::UmaHistogramEnumeration(kMetadataHistogramName, Metadata::kArtist);
  if (!metadata_.album.empty())
    base::UmaHistogramEnumeration(kMetadataHistogramName, Metadata::kAlbum);
  if (!metadata_.source_title.empty())
    base::UmaHistogramEnumeration(kMetadataHistogramName, Metadata::kSource);
}
