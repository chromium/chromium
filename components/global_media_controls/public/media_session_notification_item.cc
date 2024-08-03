// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/media_session_notification_item.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/global_media_controls/public/constants.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "media/base/remoting_constants.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

using media_session::mojom::MediaSessionAction;

namespace global_media_controls {

namespace {

media_message_center::Source GetSourceFromName(const std::string& name) {
  if (name == "web")
    return media_message_center::Source::kWeb;

  if (name == "arc")
    return media_message_center::Source::kArc;

  if (name == "assistant")
    return media_message_center::Source::kAssistant;

  return media_message_center::Source::kUnknown;
}

bool GetRemotePlaybackStarted(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  if (!base::FeatureList::IsEnabled(media::kMediaRemotingWithoutFullscreen)) {
    return false;
  }
  return session_info && session_info->remote_playback_metadata &&
         session_info->remote_playback_metadata->remote_playback_started;
}

// How long to wait (in milliseconds) for a new media session to begin.
constexpr base::TimeDelta kFreezeTimerDelay = base::Milliseconds(2500);

}  // namespace

MediaSessionNotificationItem::MediaSessionNotificationItem(
    Delegate* delegate,
    const std::string& request_id,
    const std::string& source_name,
    const std::optional<base::UnguessableToken>& source_id,
    mojo::Remote<media_session::mojom::MediaController> controller,
    media_session::mojom::MediaSessionInfoPtr session_info)
    : delegate_(delegate),
      request_id_(request_id),
      source_(GetSourceFromName(source_name)),
      source_id_(source_id) {
  DCHECK(delegate_);

  SetController(std::move(controller), std::move(session_info));
}

MediaSessionNotificationItem::~MediaSessionNotificationItem() {
  delegate_->HideItem(request_id_);
}

void MediaSessionNotificationItem::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  bool remote_playback_state_changed = GetRemotePlaybackStarted(session_info) !=
                                       GetRemotePlaybackStarted(session_info_);

  session_info_ = std::move(session_info);

  MaybeUnfreeze();
  MaybeHideOrShowNotification();

  if (view_ && !frozen_) {
    UpdateViewCommon();
    if (remote_playback_state_changed) {
      delegate_->RefreshItem(request_id_);
    }
  }
}

void MediaSessionNotificationItem::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  session_metadata_ = metadata.value_or(media_session::MediaMetadata());

  view_needs_metadata_update_ = true;

  MaybeUnfreeze();
  MaybeHideOrShowNotification();

  // |MaybeHideOrShowNotification()| can synchronously create a
  // MediaNotificationView that calls |SetView()|. If that happens, then we
  // don't want to call |view_->UpdateWithMediaMetadata()| below since |view_|
  // will have already received the metadata when calling |SetView()|.
  // |view_needs_metadata_update_| is set to false in |SetView()|. The reason we
  // want to avoid sending the metadata twice is that metrics are recorded when
  // metadata is set and we don't want to double-count metrics.
  if (view_ && view_needs_metadata_update_ && !frozen_)
    view_->UpdateWithMediaMetadata(GetSessionMetadata());

  view_needs_metadata_update_ = false;
}

void MediaSessionNotificationItem::MediaSessionActionsChanged(
    const std::vector<MediaSessionAction>& actions) {
  session_actions_ =
      base::flat_set<MediaSessionAction>(actions.begin(), actions.end());

  if (view_ && !frozen_) {
    DCHECK(view_);
    view_->UpdateWithMediaActions(GetMediaSessionActions());
  } else if (waiting_for_actions_) {
    MaybeUnfreeze();
  }
}

void MediaSessionNotificationItem::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
  session_position_ = position;
  if (!position.has_value())
    return;

  if (view_ && !frozen_) {
    view_->UpdateWithMediaPosition(*position);
  }
}

void MediaSessionNotificationItem::UpdateDeviceName(
    const std::optional<std::string>& device_name) {
  device_name_ = device_name;
  if (view_ && !frozen_) {
    view_->UpdateWithMediaMetadata(GetSessionMetadata());
    view_->UpdateWithVectorIcon(
        device_name_ ? &vector_icons::kMediaRouterIdleIcon : nullptr);
  }
}

void MediaSessionNotificationItem::UpdatePresentationRequestOrigin(
    const url::Origin& origin) {
  if (!media_message_center::IsOriginGoodForDisplay(origin)) {
    return;
  }

  optional_presentation_request_origin_ = origin;
  if (view_ && !frozen_)
    view_->UpdateWithMediaMetadata(GetSessionMetadata());
}

void MediaSessionNotificationItem::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  if (type == media_session::mojom::MediaSessionImageType::kSourceIcon) {
    session_favicon_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    if (view_ && !frozen_)
      view_->UpdateWithFavicon(*session_favicon_);

    return;
  }

  if (type == media_session::mojom::MediaSessionImageType::kChapter) {
    // Chapter images should be handled in `MediaControllerChapterImageChanged`
    // method.
    return;
  }

  DCHECK_EQ(media_session::mojom::MediaSessionImageType::kArtwork, type);

  session_artwork_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  if (view_ && !frozen_)
    view_->UpdateWithMediaArtwork(*session_artwork_);
  else if (frozen_with_artwork_)
    MaybeUnfreeze();
}

void MediaSessionNotificationItem::MediaControllerChapterImageChanged(
    int chapter_index,
    const SkBitmap& bitmap) {
  chapter_artwork_[chapter_index] = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  if (view_ && !frozen_) {
    view_->UpdateWithChapterArtwork(chapter_index,
                                    chapter_artwork_[chapter_index]);
  } else if (frozen_with_chapter_artwork_[chapter_index]) {
    MaybeUnfreeze();
  }
}

void MediaSessionNotificationItem::SetView(
    media_message_center::MediaNotificationView* view) {
  DCHECK(view_ || view);

  view_ = view;

  if (view_) {
    UpdateViewCommon();

    if (session_position_.has_value())
      view_->UpdateWithMediaPosition(*session_position_);
    if (session_artwork_.has_value())
      view_->UpdateWithMediaArtwork(*session_artwork_);
    for (auto& item : chapter_artwork_) {
      view_->UpdateWithChapterArtwork(item.first, item.second);
    }
    if (session_favicon_.has_value())
      view_->UpdateWithFavicon(*session_favicon_);
  } else {
    optional_presentation_request_origin_.reset();
  }
}

void MediaSessionNotificationItem::OnMediaSessionActionButtonPressed(
    MediaSessionAction action) {
  UMA_HISTOGRAM_ENUMERATION(kUserActionHistogramName, action);

  if (frozen_)
    return;

  delegate_->LogMediaSessionActionButtonPressed(request_id_, action);
  media_session::PerformMediaSessionAction(action, media_controller_remote_);
}

void MediaSessionNotificationItem::SeekTo(base::TimeDelta time) {
  if (!frozen_)
    media_controller_remote_->SeekTo(time);
}

void MediaSessionNotificationItem::Dismiss() {
  delegate_->RemoveItem(request_id_);
}

void MediaSessionNotificationItem::Stop() {
  if (media_controller_remote_.is_bound())
    media_controller_remote_->Stop();
}

void MediaSessionNotificationItem::Raise() {
  if (!media_controller_remote_.is_bound())
    return;

  media_controller_remote_->Raise();
}

void MediaSessionNotificationItem::SetMute(bool mute) {
  if (!frozen_)
    media_controller_remote_->SetMute(mute);
}

bool MediaSessionNotificationItem::RequestMediaRemoting() {
  if (!media_controller_remote_.is_bound())
    return false;
  media_controller_remote_->RequestMediaRemoting();
  return true;
}

media_message_center::Source MediaSessionNotificationItem::GetSource() const {
  return source_;
}

media_message_center::SourceType MediaSessionNotificationItem::GetSourceType()
    const {
  return media_message_center::SourceType::kLocalMediaSession;
}

std::optional<base::UnguessableToken>
MediaSessionNotificationItem::GetSourceId() const {
  return source_id_;
}

void MediaSessionNotificationItem::SetController(
    mojo::Remote<media_session::mojom::MediaController> controller,
    media_session::mojom::MediaSessionInfoPtr session_info) {
  observer_receiver_.reset();
  artwork_observer_receiver_.reset();
  favicon_observer_receiver_.reset();
  chapter_observer_receiver_.reset();

  is_bound_ = true;
  media_controller_remote_ = std::move(controller);
  session_info_ = std::move(session_info);

  if (media_controller_remote_.is_bound()) {
    // Bind an observer to the associated media controller.
    media_controller_remote_->AddObserver(
        observer_receiver_.BindNewPipeAndPassRemote());

    // TODO(crbug.com/40613662): Use dip to calculate the size.
    // Bind an observer to be notified when the artwork changes.
    media_controller_remote_->ObserveImages(
        media_session::mojom::MediaSessionImageType::kArtwork,
        kMediaItemArtworkMinSize, kMediaItemArtworkDesiredSize,
        artwork_observer_receiver_.BindNewPipeAndPassRemote());

    media_controller_remote_->ObserveImages(
        media_session::mojom::MediaSessionImageType::kSourceIcon,
        gfx::kFaviconSize, kMediaItemArtworkDesiredSize,
        favicon_observer_receiver_.BindNewPipeAndPassRemote());

    media_controller_remote_->ObserveImages(
        media_session::mojom::MediaSessionImageType::kChapter,
        kMediaItemArtworkMinSize, kMediaItemArtworkDesiredSize,
        chapter_observer_receiver_.BindNewPipeAndPassRemote());
  }

  MaybeHideOrShowNotification();
}

void MediaSessionNotificationItem::Freeze(base::OnceClosure unfrozen_callback) {
  is_bound_ = false;
  unfrozen_callback_ = std::move(unfrozen_callback);

  if (frozen_)
    return;

  frozen_ = true;
  frozen_with_actions_ = HasActions();
  frozen_with_artwork_ = HasArtwork();
  for (auto& item : chapter_artwork_) {
    frozen_with_chapter_artwork_[item.first] = HasChapterArtwork(item.first);
  }

  freeze_timer_.Start(
      FROM_HERE, kFreezeTimerDelay,
      base::BindOnce(&MediaSessionNotificationItem::OnFreezeTimerFired,
                     base::Unretained(this)));
}

media_session::mojom::RemotePlaybackMetadataPtr
MediaSessionNotificationItem::GetRemotePlaybackMetadata() const {
  // Return nullptr if Remote Playback is disabled or the media element is
  // encrypted.
  if (!session_info_ || !session_info_->remote_playback_metadata ||
      session_info_->remote_playback_metadata->remote_playback_disabled ||
      session_info_->remote_playback_metadata->is_encrypted_media) {
    return nullptr;
  }

  // Return nullptr if the media is too short.
  if (session_position_.has_value() &&
      session_position_.value().duration() <=
          base::Seconds(media::remoting::kMinRemotingMediaDurationInSec)) {
    return nullptr;
  }

  return session_info_->remote_playback_metadata.Clone();
}

bool MediaSessionNotificationItem::IsPlaying() const {
  return session_info_ &&
         session_info_->playback_state ==
             media_session::mojom::MediaPlaybackState::kPlaying;
}

void MediaSessionNotificationItem::FlushForTesting() {
  media_controller_remote_.FlushForTesting();  // IN-TEST
}

media_session::MediaMetadata MediaSessionNotificationItem::GetSessionMetadata()
    const {
  media_session::MediaMetadata data = session_metadata_;
  if (optional_presentation_request_origin_.has_value()) {
    data.source_title = media_message_center::GetOriginNameForDisplay(
        optional_presentation_request_origin_.value());
  }

  bool add_device_name_to_source_title = !!device_name_;
#if !BUILDFLAG(IS_CHROMEOS)
  // Never include the device name for updated media UI on non-CrOS.
  add_device_name_to_source_title &=
      !base::FeatureList::IsEnabled(media::kGlobalMediaControlsUpdatedUI);
#endif

  if (add_device_name_to_source_title) {
    std::string source_title = base::UTF16ToUTF8(data.source_title);
    const char kSeparator[] = " \xC2\xB7 ";  // "Middle dot" character.
    if (base::i18n::IsRTL()) {
      data.source_title =
          base::UTF8ToUTF16(device_name_.value() + kSeparator + source_title);
    } else {
      data.source_title =
          base::UTF8ToUTF16(source_title + kSeparator + device_name_.value());
    }
  }
  return data;
}

base::flat_set<MediaSessionAction>
MediaSessionNotificationItem::GetMediaSessionActions() const {
  if (!GetRemotePlaybackStarted(session_info_))
    return session_actions_;

  base::flat_set<MediaSessionAction> actions_without_pip(session_actions_);
  actions_without_pip.erase(MediaSessionAction::kEnterPictureInPicture);
  actions_without_pip.erase(MediaSessionAction::kExitPictureInPicture);
  return actions_without_pip;
}

bool MediaSessionNotificationItem::ShouldShowNotification() const {
  // Hide the media notification if it is not controllable or the notification
  // title is missing.
  if (!session_info_ || !session_info_->is_controllable ||
      session_metadata_.title.empty()) {
    return false;
  }

  // Hide the media notification if there exists a cast media notification item
  // for the Cast presentation. However, show the media notification if the
  // presentation is for a Remote Playback media source.
  if (session_info_->has_presentation &&
      !GetRemotePlaybackStarted(session_info_)) {
    return false;
  }

  return true;
}

void MediaSessionNotificationItem::MaybeUnfreeze() {
  if (!frozen_ && !frozen_with_artwork_ && !FrozenWithChapterArtwork()) {
    return;
  }

  if (waiting_for_actions_ && !HasActions())
    return;

  if (!ShouldShowNotification() || !is_bound_)
    return;

  // If the currently frozen view has actions and the new session currently has
  // no actions, then wait until either the freeze timer ends or the new actions
  // are received.
  if (frozen_with_actions_ && !HasActions()) {
    waiting_for_actions_ = true;
    return;
  }

  if (frozen_)
    UnfreezeNonArtwork();

  // If the currently frozen view has artwork and the new session currently has
  // no artwork, then wait until either the freeze timer ends or the new artwork
  // is downloaded.
  if (frozen_with_artwork_ && !HasArtwork()) {
    return;
  }

  for (auto& item : chapter_artwork_) {
    if (frozen_with_chapter_artwork_[item.first] &&
        !HasChapterArtwork(item.first)) {
      return;
    }
  }

  UnfreezeArtwork();
}

void MediaSessionNotificationItem::UnfreezeNonArtwork() {
  frozen_ = false;
  waiting_for_actions_ = false;
  frozen_with_actions_ = false;
  if (!frozen_with_artwork_ && !FrozenWithChapterArtwork()) {
    freeze_timer_.Stop();
  }

  // When we unfreeze, we want to fully update |view_| with any changes that
  // we've avoided sending during the freeze.
  if (view_) {
    UpdateViewCommon();
    if (session_position_.has_value())
      view_->UpdateWithMediaPosition(*session_position_);
  }

  std::move(unfrozen_callback_).Run();
}

// The artwork is frozen separately so that the rest of the UI can unfreeze
// while we await new artwork. If we didn't separate them and just didn't wait
// for the new artwork, the UI would flash between having and not having
// artwork. If we didn't separate them and did wait for new artwork, the UI
// would be slow and unresponsive when trying to skip ahead multiple tracks.
void MediaSessionNotificationItem::UnfreezeArtwork() {
  frozen_with_artwork_ = false;
  for (auto& item : chapter_artwork_) {
    frozen_with_chapter_artwork_[item.first] = false;
  }
  freeze_timer_.Stop();
  if (view_) {
    if (session_artwork_.has_value())
      view_->UpdateWithMediaArtwork(*session_artwork_);
    if (session_favicon_.has_value())
      view_->UpdateWithFavicon(*session_favicon_);
    for (auto& item : chapter_artwork_) {
      view_->UpdateWithChapterArtwork(item.first, item.second);
    }
  }
}

bool MediaSessionNotificationItem::HasActions() const {
  return !session_actions_.empty();
}

bool MediaSessionNotificationItem::HasArtwork() const {
  return session_artwork_.has_value() && !session_artwork_->isNull();
}

bool MediaSessionNotificationItem::HasChapterArtwork(int index) const {
  auto it = chapter_artwork_.find(index);
  return it != chapter_artwork_.end() && !it->second.isNull();
}

void MediaSessionNotificationItem::OnFreezeTimerFired() {
  DCHECK(frozen_ || frozen_with_artwork_ || FrozenWithChapterArtwork());

  // If we've just been waiting for actions or artwork, stop waiting and just
  // show what we have.
  if (ShouldShowNotification() && is_bound_) {
    if (frozen_)
      UnfreezeNonArtwork();

    if (frozen_with_artwork_ || FrozenWithChapterArtwork()) {
      UnfreezeArtwork();
    }

    return;
  }

  if (is_bound_) {
    delegate_->HideItem(request_id_);
  } else {
    delegate_->RemoveItem(request_id_);
  }
}

void MediaSessionNotificationItem::MaybeHideOrShowNotification() {
  if (frozen_)
    return;

  if (!ShouldShowNotification()) {
    delegate_->HideItem(request_id_);
    return;
  }

  // If we have an existing view, then we don't need to create a new one.
  if (view_)
    return;

  delegate_->ActivateItem(request_id_);
}

void MediaSessionNotificationItem::UpdateViewCommon() {
  view_needs_metadata_update_ = false;
  view_->UpdateWithMediaSessionInfo(session_info_);
  view_->UpdateWithMediaMetadata(GetSessionMetadata());
  view_->UpdateWithMediaActions(GetMediaSessionActions());
  view_->UpdateWithMuteStatus(session_info_->muted);
  view_->UpdateWithVectorIcon(device_name_ ? &vector_icons::kMediaRouterIdleIcon
                                           : nullptr);
}

bool MediaSessionNotificationItem::FrozenWithChapterArtwork() {
  auto it =
      base::ranges::find_if(frozen_with_chapter_artwork_,
                            [](const auto& it) { return it.second == true; });
  return it != frozen_with_chapter_artwork_.end();
}
}  // namespace global_media_controls
