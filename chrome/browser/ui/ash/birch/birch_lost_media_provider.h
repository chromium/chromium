// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LOST_MEDIA_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LOST_MEDIA_PROVIDER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "components/favicon_base/favicon_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

class Profile;

namespace ash {

class VideoConferenceTrayController;

// Manages fetching tabs that are playing media by subscribing to
// MediaControllerObserver. This provider will return the most recent last
// active tab the user was on if there are multiple tabs playing media. When the
// `BirchVideoConferenceSuggestions` flag is turned on, the provider will also
// return active video conference tabs.
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

  // This callback executes once video conference app data is available from the
  // `video_conference_controller_`, and passes vc app data to the birch model.
  void OnVideoConferencingDataAvailable(
      VideoConferenceManagerAsh::MediaApps media_apps);

  // Passes media apps data that is observed from MediaSessionMetaDataChanged()
  // to the birch model.
  void SetMediaAppsFromMediaController();

  // Handles activation events by either returning to a specific video
  // conference session or calling `Raise()` on the media controller to activate
  // the correct media tab. A valid `vc_id` is required to return to the vc
  // session; otherwise, we can infer that the item is a media tab.
  void OnItemPressed(std::optional<base::UnguessableToken> vc_id);

  void set_fake_media_controller_for_testing(
      mojo::Remote<media_session::mojom::MediaController> fake_controller) {
    media_controller_remote_ = std::move(fake_controller);
  }

  void set_fake_video_conference_controller_for_testing(
      VideoConferenceTrayController* fake_controller) {
    video_conference_controller_ = fake_controller;
  }

  // The media controller that is responsible for executing media actions.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  // The mojo receiver to observe changes from `MediaControllerObserver`.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_observer_receiver_{this};

  // Whether the media is playing (it might be paused or stopped).
  bool is_playing_ = false;

  // The title of the media item.
  std::u16string media_title_;

  // The origin source of the media item. This will assist in the creation of a
  // complete GURL source on birch item creation.
  std::u16string source_url_;

  // The type of the secondary icon. Default to no icon type.
  SecondaryIconType secondary_icon_type_ = SecondaryIconType::kNoIcon;

  // `VideoConferenceTrayController` used to get active video conference
  // session.
  raw_ptr<VideoConferenceTrayController> video_conference_controller_;

  // Used for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<BirchLostMediaProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LOST_MEDIA_PROVIDER_H_
