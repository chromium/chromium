// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_lost_media_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/branding_buildflags.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "services/media_session/public/mojom/media_session.mojom-shared.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/preinstalled_web_apps_resources.h"
#endif

namespace ash {
namespace {

// Returns true if focus mode is playing media (e.g. an audio playlist).
bool IsFocusModePlayingMedia() {
  if (!features::IsFocusModeEnabled()) {
    return false;
  }
  const focus_mode_util::SelectedPlaylist& playlist =
      FocusModeController::Get()
          ->focus_mode_sounds_controller()
          ->selected_playlist();
  return !playlist.empty() &&
         playlist.state == focus_mode_util::SoundState::kPlaying;
}

}  // namespace

BirchLostMediaProvider::BirchLostMediaProvider(Profile* profile)
    : profile_(profile) {
  if (!media_controller_remote_.is_bound()) {
    media_session::MediaSessionService* service =
        Shell::Get()->shell_delegate()->GetMediaSessionService();
    if (!service) {
      return;
    }
    // Connect to the MediaControllerManager and create a MediaController that
    // controls the active session so we can observe it.
    mojo::Remote<media_session::mojom::MediaControllerManager>
        controller_manager_remote;
    service->BindMediaControllerManager(
        controller_manager_remote.BindNewPipeAndPassReceiver());
    controller_manager_remote->CreateActiveMediaController(
        media_controller_remote_.BindNewPipeAndPassReceiver());

    media_controller_remote_->AddObserver(
        media_observer_receiver_.BindNewPipeAndPassRemote());
  }
  if (features::IsBirchVideoConferenceSuggestionsEnabled()) {
    video_conference_controller_ = VideoConferenceTrayController::Get();
  }
}

BirchLostMediaProvider::~BirchLostMediaProvider() = default;

void BirchLostMediaProvider::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  std::optional<media_session::MediaMetadata> pending_metadata =
      metadata.value_or(media_session::MediaMetadata());
  if (pending_metadata.has_value()) {
    media_title_ = pending_metadata->title;
    source_url_ = pending_metadata->source_title;
  }

  NotifyDataProviderChanged();
}

void BirchLostMediaProvider::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  // Notify data changed on closure.
  base::ScopedClosureRunner scoped_closure(
      base::BindOnce(&BirchLostMediaProvider::NotifyDataProviderChanged,
                     base::Unretained(this)));

  media_session::mojom::MediaSessionInfoPtr media_session_info =
      std::move(session_info);

  if (!media_session_info) {
    secondary_icon_type_ = SecondaryIconType::kNoIcon;
    return;
  }

  is_playing_ = media_session_info->playback_state ==
                media_session::mojom::MediaPlaybackState::kPlaying;

  if (media_session_info->audio_video_states.has_value() &&
      !media_session_info->audio_video_states->empty()) {
    auto& first_state = media_session_info->audio_video_states->at(0);
    switch (first_state) {
      case media_session::mojom::MediaAudioVideoState::kAudioOnly:
        secondary_icon_type_ = SecondaryIconType::kLostMediaAudio;
        return;
      case media_session::mojom::MediaAudioVideoState::kAudioVideo:
      case media_session::mojom::MediaAudioVideoState::kVideoOnly:
        secondary_icon_type_ = SecondaryIconType::kLostMediaVideo;
        return;
      default:
        secondary_icon_type_ = SecondaryIconType::kNoIcon;
    }
  }
}

void BirchLostMediaProvider::RequestBirchDataFetch() {
  if (video_conference_controller_) {
    video_conference_controller_->GetMediaApps(base::BindOnce(
        &BirchLostMediaProvider::OnVideoConferencingDataAvailable,
        weak_factory_.GetWeakPtr()));
    return;
  }

  // If `video_conference_controller_` doesn't exist, then
  // skip setting vc apps and call to set media apps instead.
  SetMediaAppsFromMediaController();
}

void BirchLostMediaProvider::OnVideoConferencingDataAvailable(
    VideoConferenceManagerAsh::MediaApps apps) {
  std::vector<BirchLostMediaItem> items;

  // If video conference apps exist, return the most recently active app data to
  // the birch model.
  if (!apps.empty()) {
    items.emplace_back(
        /*source_url=*/apps[0]->url.value_or(GURL()),
        /*media_title=*/apps[0]->title,
        /*backup_icon=*/std::nullopt,
        /*secondary_icon_type=*/SecondaryIconType::kLostMediaVideoConference,
        /*activation_callback=*/
        base::BindRepeating(&BirchLostMediaProvider::OnItemPressed,
                            weak_factory_.GetWeakPtr(), apps[0]->id));
    Shell::Get()->birch_model()->SetLostMediaItems(std::move(items));
    return;
  }

  // If video conference apps doesn't exist, then call to set media apps.
  SetMediaAppsFromMediaController();
}

void BirchLostMediaProvider::SetMediaAppsFromMediaController() {
  // Returns early if no media controller is bound or if pertinent media app
  // details are missing.
  if (!media_controller_remote_.is_bound() || !is_playing_ ||
      media_title_.empty() || source_url_.empty()) {
    Shell::Get()->birch_model()->SetLostMediaItems({});
    return;
  }

  // If focus mode is playing audio, don't show the lost media suggestion, since
  // there's no tab associated with the focus mode audio.
  if (IsFocusModePlayingMedia()) {
    Shell::Get()->birch_model()->SetLostMediaItems({});
    return;
  }

  std::optional<ui::ImageModel> backup_icon;
  // The YouTube icon is only available in branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (source_url_.starts_with(u"youtube.com")) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    backup_icon = ui::ImageModel::FromImageSkia(
        *rb.GetImageSkiaNamed(IDR_PREINSTALLED_WEB_APPS_YOUTUBE_ICON_192_PNG));
  }
#endif

  std::vector<BirchLostMediaItem> items;
  // `source_url_` doesn't contain necessary prefix to make it a valid GURL so
  // we must append a prefix to it.
  items.emplace_back(
      /*source_url=*/GURL(u"https://www." + source_url_),
      /*media_title=*/media_title_,
      /*backup_icon=*/backup_icon,
      /*secondary_icon_type=*/secondary_icon_type_,
      /*activation_callback=*/
      base::BindRepeating(&BirchLostMediaProvider::OnItemPressed,
                          weak_factory_.GetWeakPtr(), std::nullopt));
  Shell::Get()->birch_model()->SetLostMediaItems(std::move(items));
}

void BirchLostMediaProvider::OnItemPressed(
    std::optional<base::UnguessableToken> vc_id) {
  if (vc_id.has_value()) {
    if (video_conference_controller_) {
      video_conference_controller_->ReturnToApp(vc_id.value());
    }
  } else {
    media_controller_remote_->Raise();
  }
}

void BirchLostMediaProvider::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {}

void BirchLostMediaProvider::MediaSessionChanged(
    const std::optional<base::UnguessableToken>& request_id) {}

void BirchLostMediaProvider::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {}

}  // namespace ash
