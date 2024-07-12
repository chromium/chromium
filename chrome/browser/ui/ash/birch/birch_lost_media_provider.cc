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
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "components/favicon/core/favicon_service.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

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

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const ui::ImageModel backup_icon = ui::ImageModel::FromImageSkia(
      *rb.GetImageSkiaNamed(IDR_CHROME_APP_ICON_192));

  // If video conference apps exist, return the most recently active app data to
  // the birch model.
  if (!apps.empty()) {
    items.emplace_back(
        /*source_url=*/apps[0]->url.value_or(GURL()),
        /*media_title=*/apps[0]->title,
        /*is_video_conference_tab=*/true,
        /*backup_icon=*/backup_icon,
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
  if (!media_controller_remote_.is_bound() || media_title_.empty() ||
      source_url_.empty()) {
    Shell::Get()->birch_model()->SetLostMediaItems({});
    return;
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const ui::ImageModel backup_icon = ui::ImageModel::FromImageSkia(
      *rb.GetImageSkiaNamed(IDR_CHROME_APP_ICON_192));

  std::vector<BirchLostMediaItem> items;
  // `source_url_` doesn't contain necessary prefix to make it a valid GURL so
  // we must append a prefix to it.
  items.emplace_back(
      /*source_url=*/GURL(u"https://www." + source_url_),
      /*media_title=*/media_title_,
      /*is_video_conference_tab=*/false,
      /*backup_icon=*/backup_icon,
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

void BirchLostMediaProvider::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {}

void BirchLostMediaProvider::MediaSessionChanged(
    const std::optional<base::UnguessableToken>& request_id) {}

void BirchLostMediaProvider::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {}

}  // namespace ash
