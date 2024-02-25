// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_SUPPLEMENTAL_DEVICE_PICKER_ITEM_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_SUPPLEMENTAL_DEVICE_PICKER_ITEM_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "content/public/browser/presentation_request.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

// See the class comment for SupplementalDevicePickerProducer for more
// information.
class SupplementalDevicePickerItem final
    : public media_message_center::MediaNotificationItem {
 public:
  SupplementalDevicePickerItem(
      global_media_controls::MediaItemManager* item_manager,
      const base::UnguessableToken& source_id);
  SupplementalDevicePickerItem(const SupplementalDevicePickerItem&) = delete;
  SupplementalDevicePickerItem& operator=(const SupplementalDevicePickerItem&) =
      delete;
  ~SupplementalDevicePickerItem() final;

  // media_message_center::MediaNotificationItem
  void Dismiss() final;

  // Updates `view_` with the new info.
  void UpdateViewWithMetadata(const media_session::MediaMetadata& metadata);
  void UpdateViewWithArtworkImage(std::optional<gfx::ImageSkia> artwork_image);
  void UpdateViewWithFaviconImage(std::optional<gfx::ImageSkia> favicon_image);

  // media_message_center::MediaNotificationItem
  void SetView(media_message_center::MediaNotificationView* view) final;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) final {}
  void SeekTo(base::TimeDelta time) final {}
  void SetVolume(float volume) override {}
  void SetMute(bool mute) override {}
  bool RequestMediaRemoting() override;
  media_message_center::Source GetSource() const override;
  media_message_center::SourceType GetSourceType() const override;
  std::optional<base::UnguessableToken> GetSourceId() const override;

  base::WeakPtr<SupplementalDevicePickerItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const std::string& id() const { return id_; }

 private:
  void UpdateViewWithMetadata();
  void UpdateViewWithImages();

  const std::string id_;
  const raw_ptr<global_media_controls::MediaItemManager> item_manager_;

  // The metadata for the Media Session associated with the WebContents that
  // this presentation request is associated with.
  media_session::MediaMetadata metadata_;

  // An image for the Media Session associated with the
  // WebContents this presentation request is associated with.
  std::optional<gfx::ImageSkia> artwork_image_;
  std::optional<gfx::ImageSkia> favicon_image_;

  raw_ptr<media_message_center::MediaNotificationView> view_ = nullptr;

  const base::UnguessableToken source_id_;

  base::WeakPtrFactory<SupplementalDevicePickerItem> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_SUPPLEMENTAL_DEVICE_PICKER_ITEM_H_
