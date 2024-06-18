// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_lost_media_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "components/favicon/core/favicon_service.h"
#include "services/media_session/public/cpp/media_session_service.h"

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
}

BirchLostMediaProvider::~BirchLostMediaProvider() = default;

void BirchLostMediaProvider::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  std::optional<media_session::MediaMetadata> pending_metadata =
      metadata.value_or(media_session::MediaMetadata());
  if (pending_metadata.has_value()) {
    media_title_ = pending_metadata->title;
    source_title_ = pending_metadata->source_title;
  }
}

void BirchLostMediaProvider::RequestBirchDataFetch() {
  if (!media_controller_remote_.is_bound()) {
    Shell::Get()->birch_model()->SetLostMediaItems({});
    return;
  }

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service) {
    return;
  }

  if (media_title_.empty() || source_title_.empty()) {
    Shell::Get()->birch_model()->SetLostMediaItems({});
    return;
  }

  TempMediaItem temp_item(source_title_, media_title_);
  // source title doesn't contain necessary prefix to make it a valid URL so we
  // must append to it.
  const GURL website_url = GURL(u"https://www." + source_title_);
  favicon_service->GetFaviconImageForPageURL(
      website_url,
      base::BindOnce(&BirchLostMediaProvider::OnFavIconDataAvailable,
                     weak_factory_.GetWeakPtr(), temp_item),
      &cancelable_task_tracker_);
}

void BirchLostMediaProvider::OnFavIconDataAvailable(
    const TempMediaItem& temp_item,
    const favicon_base::FaviconImageResult& image_result) {
  std::vector<BirchLostMediaItem> items;

  items.emplace_back(temp_item.source_title, temp_item.media_title,
                     (image_result.image.IsEmpty()
                          ? ui::ImageModel()
                          : ui::ImageModel::FromImage(image_result.image)),
                     base::BindRepeating(&BirchLostMediaProvider::OnItemPressed,
                                         weak_factory_.GetWeakPtr()));

  Shell::Get()->birch_model()->SetLostMediaItems(std::move(items));
}

void BirchLostMediaProvider::OnItemPressed() {
  media_controller_remote_->Raise();
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
