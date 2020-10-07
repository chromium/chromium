// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_SESSION_NOTIFICATION_ITEM_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_SESSION_NOTIFICATION_ITEM_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/media_message_center/media_notification_item.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace media_message_center {

class MediaNotificationController;
class MediaNotificationView;

// MediaSessionNotificationItem manages hiding/showing a media notification and
// updating the metadata for a single media session.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaSessionNotificationItem
    : public MediaNotificationItem,
      public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver {
 public:
  MediaSessionNotificationItem(
      MediaNotificationController* notification_controller,
      const std::string& request_id,
      const std::string& source_name,
      mojo::Remote<media_session::mojom::MediaController> controller,
      media_session::mojom::MediaSessionInfoPtr session_info);
  MediaSessionNotificationItem(const MediaSessionNotificationItem&) = delete;
  MediaSessionNotificationItem& operator=(const MediaSessionNotificationItem&) =
      delete;
  ~MediaSessionNotificationItem() override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;

  // MediaNotificationItem:
  void SetView(MediaNotificationView* view) override;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) override;
  // This will stop the media session associated with this item. The item will
  // then call |MediaNotificationController::RemoveItem()| to ensure removal.
  void Dismiss() override;
  media_message_center::SourceType SourceType() override;

  base::WeakPtr<MediaSessionNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetController(
      mojo::Remote<media_session::mojom::MediaController> controller,
      media_session::mojom::MediaSessionInfoPtr session_info);

  // This will freeze the item and start a timer to destroy the item after
  // some time has passed. If and when the item unfreezes, |unfrozen_callback|
  // will be run. If the item does not unfreeze before timing out, then
  // |unfrozen_callback| will not be called.
  void Freeze(base::OnceClosure unfrozen_callback);

  bool frozen() const { return frozen_; }

  void FlushForTesting();

  void SetMediaControllerForTesting(
      mojo::Remote<media_session::mojom::MediaController> controller) {
    media_controller_remote_ = std::move(controller);
  }

 private:
  bool ShouldShowNotification() const;

  void MaybeUnfreeze();

  void Unfreeze();

  bool HasActions() const;

  bool HasArtwork() const;

  void OnFreezeTimerFired();

  void MaybeHideOrShowNotification();

  MediaNotificationController* controller_;

  bool is_bound_ = true;

  // Weak reference to the view of the currently shown media notification.
  MediaNotificationView* view_ = nullptr;

  // The |request_id_| is the request id of the media session and is guaranteed
  // to be globally unique.
  const std::string request_id_;

  // The source of the media session (e.g. arc, web).
  const Source source_;

  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  media_session::mojom::MediaSessionInfoPtr session_info_;

  media_session::MediaMetadata session_metadata_;

  base::flat_set<media_session::mojom::MediaSessionAction> session_actions_;

  base::Optional<gfx::ImageSkia> session_artwork_;

  base::Optional<gfx::ImageSkia> session_favicon_;

  // True if the metadata needs to be updated on |view_|. Used to prevent
  // updating |view_|'s metadata twice on a single change.
  bool view_needs_metadata_update_ = false;

  // When the item is frozen the |view_| will not receive any updates to the
  // data and no actions will be executed.
  bool frozen_ = false;

  // True if we're currently frozen and the frozen view contains at least 1
  // action.
  bool frozen_with_actions_ = false;

  // True if we have the necessary metadata to unfreeze, but we're waiting for
  // new actions.
  bool waiting_for_actions_ = false;

  // True if we're currently frozen and the frozen view contains non-null
  // artwork.
  bool frozen_with_artwork_ = false;

  // True if we have the necessary metadata to unfreeze, but we're waiting for
  // new artwork to load.
  bool waiting_for_artwork_ = false;

  // The timer that will notify the controller to destroy this item after it
  // has been frozen for a certain period of time.
  base::OneShotTimer freeze_timer_;

  // Called when the item unfreezes.
  base::OnceClosure unfrozen_callback_;

  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      observer_receiver_{this};

  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      artwork_observer_receiver_{this};

  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      favicon_observer_receiver_{this};

  base::WeakPtrFactory<MediaSessionNotificationItem> weak_ptr_factory_{this};
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_SESSION_NOTIFICATION_ITEM_H_
