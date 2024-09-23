// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"
#include "components/global_media_controls/public/constants.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/cpp/media_metadata.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class Profile;

// Represents the media notification shown in the Global Media Controls dialog
// for a Cast session. It is responsible for showing/hiding a
// MediaNotificationView.
class CastMediaNotificationItem
    : public media_message_center::MediaNotificationItem,
      public media_router::mojom::MediaStatusObserver {
 public:
  using BitmapFetcherFactory =
      base::RepeatingCallback<std::unique_ptr<BitmapFetcher>(
          const GURL&,
          BitmapFetcherDelegate*,
          const net::NetworkTrafficAnnotationTag&)>;

  CastMediaNotificationItem(
      const media_router::MediaRoute& route,
      global_media_controls::MediaItemManager* item_manager,
      std::unique_ptr<CastMediaSessionController> session_controller,
      Profile* profile);
  CastMediaNotificationItem(const CastMediaNotificationItem&) = delete;
  CastMediaNotificationItem& operator=(const CastMediaNotificationItem&) =
      delete;
  ~CastMediaNotificationItem() override;

  // media_message_center::MediaNotificationItem:
  void SetView(media_message_center::MediaNotificationView* view) override;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) override;
  void SeekTo(base::TimeDelta time) override;
  void Dismiss() override;
  void SetVolume(float volume) override;
  void SetMute(bool mute) override;
  bool RequestMediaRemoting() override;
  media_message_center::Source GetSource() const override;
  media_message_center::SourceType GetSourceType() const override;
  std::optional<base::UnguessableToken> GetSourceId() const override;

  // media_router::mojom::MediaStatusObserver:
  void OnMediaStatusUpdated(
      media_router::mojom::MediaStatusPtr status) override;

  void OnRouteUpdated(const media_router::MediaRoute& route);

  // Stops the cast session and logs UMA about the stop cast action.
  virtual void StopCasting();

  // Returns a pending remote bound to |this|. This should not be called more
  // than once per instance.
  mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
  GetObserverPendingRemote();

  const media_router::MediaRoute::Id route_id() const {
    return media_route_id_;
  }
  Profile* profile() { return profile_; }
  bool is_active() const { return is_active_; }
  bool route_is_local() const { return route_is_local_; }
  std::optional<std::string> device_name() const { return device_name_; }

  base::WeakPtr<CastMediaNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void set_bitmap_fetcher_factory_for_testing_(BitmapFetcherFactory factory) {
    image_downloader_.set_bitmap_fetcher_factory_for_testing(
        std::move(factory));
  }

 private:
  class ImageDownloader : public BitmapFetcherDelegate {
   public:
    ImageDownloader(Profile* profile,
                    base::RepeatingCallback<void(const SkBitmap&)> callback);
    ImageDownloader(const ImageDownloader&) = delete;
    ImageDownloader& operator=(const ImageDownloader&) = delete;
    ~ImageDownloader() override;

    // BitmapFetcherDelegate:
    void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

    // Downloads the image specified by |url| and passes the result to
    // |callback_|. No-ops if |url| is the same as the previous call.
    void Download(const GURL& url);

    // Resets the bitmap fetcher, the saved image, and the image URL.
    void Reset();

    const SkBitmap& bitmap() const { return bitmap_; }

    void set_bitmap_fetcher_factory_for_testing(BitmapFetcherFactory factory) {
      bitmap_fetcher_factory_for_testing_ = std::move(factory);
    }

   private:
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    base::RepeatingCallback<void(const SkBitmap&)> callback_;
    std::unique_ptr<BitmapFetcher> bitmap_fetcher_;
    GURL url_;
    // The downloaded bitmap.
    SkBitmap bitmap_;

    BitmapFetcherFactory bitmap_fetcher_factory_for_testing_;
  };

  void UpdateView();
  void ImageChanged(const SkBitmap& bitmap);

  // The notification is shown when active.
  bool is_active_ = true;

  const raw_ptr<global_media_controls::MediaItemManager> item_manager_;
  raw_ptr<media_message_center::MediaNotificationView> view_ = nullptr;
  const raw_ptr<Profile> profile_;

  std::unique_ptr<CastMediaSessionController> session_controller_;
  const media_router::MediaRoute::Id media_route_id_;
  // True if the route is started from the |profile_| on the current device.
  const bool route_is_local_;
  std::optional<std::string> device_name_;
  ImageDownloader image_downloader_;
  media_session::MediaMetadata metadata_;
  std::vector<media_session::mojom::MediaSessionAction> actions_;
  media_session::mojom::MediaSessionInfoPtr session_info_;
  media_session::MediaPosition media_position_;
  bool is_muted_ = false;
  float volume_ = 0.0;
  mojo::Receiver<media_router::mojom::MediaStatusObserver> observer_receiver_{
      this};
  base::WeakPtrFactory<CastMediaNotificationItem> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_
