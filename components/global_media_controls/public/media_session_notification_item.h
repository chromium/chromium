// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_NOTIFICATION_ITEM_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_NOTIFICATION_ITEM_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/media_message_center/media_notification_item.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

namespace media_message_center {
class MediaNotificationView;
}  // namespace media_message_center

namespace global_media_controls {

// MediaSessionNotificationItem manages hiding/showing a media notification and
// updating the metadata for a single media session.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaSessionNotificationItem
    : public media_message_center::MediaNotificationItem,
      public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver {
 public:
  class Delegate {
   public:
    // The given item meets the criteria for being displayed.
    virtual void ActivateItem(const std::string& id) = 0;

    // The given item no longer meets the criteria for being displayed, but may
    // be reactivated.
    virtual void HideItem(const std::string& id) = 0;

    // The given item should be destroyed.
    virtual void RemoveItem(const std::string& id) = 0;

    // The given item's UI should be refreshed.
    virtual void RefreshItem(const std::string& id) = 0;

    // The given button has been pressed, and therefore the action should be
    // recorded.
    virtual void LogMediaSessionActionButtonPressed(
        const std::string& id,
        media_session::mojom::MediaSessionAction action) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  MediaSessionNotificationItem(
      Delegate* delegate,
      const std::string& request_id,
      const std::string& source_name,
      const std::optional<base::UnguessableToken>& source_id,
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
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

  // Called when a media session item is associated with a presentation request
  // to show the origin associated with the request rather than that for the
  // top frame.
  void UpdatePresentationRequestOrigin(const url::Origin& origin);

  // Called during the creation of the footer view to show / set sink name if
  // there is an active casting session associated with `this` media item.
  void UpdateDeviceName(const std::optional<std::string>& device_name);

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;
  void MediaControllerChapterImageChanged(int chapter_index,
                                          const SkBitmap& bitmap) override;

  // media_message_center::MediaNotificationItem:
  void SetView(media_message_center::MediaNotificationView* view) override;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) override;
  void SeekTo(base::TimeDelta time) override;
  // This will stop the media session associated with this item. The item will
  // then call |MediaNotificationController::RemoveItem()| to ensure removal.
  void Dismiss() override;
  void SetVolume(float volume) override {}
  void SetMute(bool mute) override;
  bool RequestMediaRemoting() override;
  media_message_center::Source GetSource() const override;
  media_message_center::SourceType GetSourceType() const override;
  std::optional<base::UnguessableToken> GetSourceId() const override;

  // Stops the media session.
  void Stop();

  // Calls |Raise()| on the underlying MediaSession, which will focus the
  // WebContents if the MediaSession is associated with one.
  void Raise();

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

  std::optional<std::string> device_name() const { return device_name_; }

  // Returns nullptr if `remote_playback_disabled` is true in `session_info_` or
  // the media duration is too short.
  media_session::mojom::RemotePlaybackMetadataPtr GetRemotePlaybackMetadata()
      const;

  // Returns whether the item is actively playing.
  bool IsPlaying() const;

  void FlushForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaSessionNotificationItemTest,
                           GetSessionMetadata);
#if !BUILDFLAG(IS_CHROMEOS)
  FRIEND_TEST_ALL_PREFIXES(MediaSessionNotificationItemTest,
                           GetSessionMetadataForUpdatedUI);
#endif
  FRIEND_TEST_ALL_PREFIXES(MediaSessionNotificationItemTest,
                           GetMediaSessionActions);
  FRIEND_TEST_ALL_PREFIXES(MediaSessionNotificationItemTest,
                           ShouldShowNotification);

  media_session::MediaMetadata GetSessionMetadata() const;
  base::flat_set<media_session::mojom::MediaSessionAction>
  GetMediaSessionActions() const;

  bool ShouldShowNotification() const;

  void MaybeUnfreeze();

  void UnfreezeNonArtwork();

  void UnfreezeArtwork();

  bool HasActions() const;

  bool HasArtwork() const;

  // Returns true if there's an image at the chapter `index`.
  bool HasChapterArtwork(int index) const;

  void OnFreezeTimerFired();

  void MaybeHideOrShowNotification();

  void UpdateViewCommon();

  // Returns true if we're currently frozen and the frozen view contains
  // non-null artwork at any chapter index.
  bool FrozenWithChapterArtwork();

  const raw_ptr<Delegate> delegate_;

  bool is_bound_ = true;

  // Weak reference to the view of the currently shown media notification.
  raw_ptr<media_message_center::MediaNotificationView> view_ = nullptr;

  // The |request_id_| is the request id of the media session and is guaranteed
  // to be globally unique.
  const std::string request_id_;

  // The source of the media session.
  const media_message_center::Source source_;

  // The ID assigned to `source_`.
  std::optional<base::UnguessableToken> source_id_;

  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  media_session::mojom::MediaSessionInfoPtr session_info_;

  media_session::MediaMetadata session_metadata_;

  // When a media session item is associated with a presentation request, we
  // must show the origin associated with the request rather than that for the
  // top frame. So, in case of having a presentation request, this field is set
  // to hold the origin of that presentation request.
  std::optional<url::Origin> optional_presentation_request_origin_;

  // This name is used when the playback is happening on a non-default playback
  // device.
  std::optional<std::string> device_name_;

  base::flat_set<media_session::mojom::MediaSessionAction> session_actions_;

  std::optional<media_session::MediaPosition> session_position_;

  std::optional<gfx::ImageSkia> session_artwork_;

  std::optional<gfx::ImageSkia> session_favicon_;

  // This map carries the index as the key and the image at this chapter index
  // as the value.
  base::flat_map<int, gfx::ImageSkia> chapter_artwork_;

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

  // The value is true if we're currently frozen and the frozen view contains
  // non-null artwork at the chapter index.
  base::flat_map<int, bool> frozen_with_chapter_artwork_;

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

  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      chapter_observer_receiver_{this};

  base::WeakPtrFactory<MediaSessionNotificationItem> weak_ptr_factory_{this};
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_NOTIFICATION_ITEM_H_
