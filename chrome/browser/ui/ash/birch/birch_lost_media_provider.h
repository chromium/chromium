// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LOST_MEDIA_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LOST_MEDIA_PROVIDER_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

class Profile;

namespace ash {

// Manages fetching tabs that are playing media by subscribing to
// MediaControllerObserver. This provider will return the most recent last
// active tab the user was on if there are multiple tabs playing media.
class ASH_EXPORT BirchLostMediaProvider
    : public BirchDataProvider,
      public media_session::mojom::MediaControllerObserver {
 public:
  explicit BirchLostMediaProvider(Profile* profile);
  BirchLostMediaProvider(const BirchLostMediaProvider&) = delete;
  BirchLostMediaProvider& operator=(const BirchLostMediaProvider&) = delete;
  ~BirchLostMediaProvider() override;

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override;
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

 private:
  friend class BirchKeyedServiceTest;

  // A struct to help with BirchLostMediaItem creation.
  struct TempMediaItem {
    std::u16string source_title;
    std::u16string media_title;
    TempMediaItem(const std::u16string& source_title,
                  const std::u16string& media_title)
        : source_title(source_title), media_title(media_title) {}
  };

  void OnFavIconDataAvailable(
      const TempMediaItem& temp_item,
      const favicon_base::FaviconImageResult& image_result);

  void OnItemPressed();

  void set_media_controller_for_testing(
      mojo::Remote<media_session::mojom::MediaController> controller) {
    media_controller_remote_ = std::move(controller);
  }

  // The media controller that is responsible for executing media actions.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  // The mojo receiver to observe changes from `MediaControllerObserver`.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_observer_receiver_{this};

  // The title of the media item.
  std::u16string media_title_;

  // The origin source of the media item. This will assist in the creation of a
  // complete GURL source on birch item creation.
  std::u16string source_title_;

  // Used for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<BirchLostMediaProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LOST_MEDIA_PROVIDER_H_
