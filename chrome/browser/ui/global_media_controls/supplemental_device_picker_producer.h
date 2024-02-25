// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_SUPPLEMENTAL_DEVICE_PICKER_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_SUPPLEMENTAL_DEVICE_PICKER_PRODUCER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_item.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer_set.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

// This object creates and manages media dialog items (represented by
// SupplementalDevicePicker) that contain device pickers not associated with any
// active media. Such an item becomes necessary e.g. when wanting to
// present/cast from a web page with no active media.
//
// This object is not the only producer of items containing device pickers.
// MediaSessionItemProducer produces items containing pickers for active media.
//
// On Chrome OS, this object lives on the Ash side of the Ash-Lacros split.
class SupplementalDevicePickerProducer final
    : public global_media_controls::MediaItemProducer,
      public global_media_controls::MediaItemManagerObserver,
      public global_media_controls::MediaItemUIObserver,
      public global_media_controls::mojom::DevicePickerProvider {
 public:
  explicit SupplementalDevicePickerProducer(
      global_media_controls::MediaItemManager* item_manager);
  SupplementalDevicePickerProducer(const SupplementalDevicePickerProducer&) =
      delete;
  SupplementalDevicePickerProducer& operator=(
      const SupplementalDevicePickerProducer&) = delete;
  ~SupplementalDevicePickerProducer() final;

  // global_media_controls::MediaItemProducer:
  base::WeakPtr<media_message_center::MediaNotificationItem> GetMediaItem(
      const std::string& id) override;
  std::set<std::string> GetActiveControllableItemIds() const override;
  bool HasFrozenItems() override;
  void OnItemShown(const std::string& id,
                   global_media_controls::MediaItemUI* item_ui) override;
  bool IsItemActivelyPlaying(const std::string& id) override;

  // global_media_controls::MediaItemUIObserver:
  void OnMediaItemUIDismissed(const std::string& id) override;

  // global_media_controls::mojom::DevicePickerProvider:
  void CreateItem(const base::UnguessableToken& source_id) override;
  void DeleteItem() override;
  void ShowItem() override;
  void HideItem() override;
  void OnMetadataChanged(const media_session::MediaMetadata& metadata) override;
  void OnArtworkImageChanged(const gfx::ImageSkia& artwork_image) override;
  void OnFaviconImageChanged(const gfx::ImageSkia& favicon_image) override;
  void AddObserver(
      mojo::PendingRemote<global_media_controls::mojom::DevicePickerObserver>
          observer) override;
  void HideMediaUI() override;

  // Returns the item managed by `this`. Creates one if it doesn't already
  // exist. `source_id` is the per-Profile MediaSession source ID used for
  // distinguishing callers.
  const SupplementalDevicePickerItem& GetOrCreateNotificationItem(
      const base::UnguessableToken& source_id);

  // Returns a remote bound to `this`.
  mojo::PendingRemote<global_media_controls::mojom::DevicePickerProvider>
  PassRemote();

  // global_media_controls::MediaItemManagerObserver:
  void OnItemListChanged() final;
  void OnMediaDialogOpened() final;
  void OnMediaDialogClosed() final;

 private:
  friend class SupplementalDevicePickerProducerTest;
  class PresentationRequestWebContentsObserver;

  const raw_ptr<global_media_controls::MediaItemManager> item_manager_;

  // The notification managed by this producer, if there is one.
  std::optional<SupplementalDevicePickerItem> item_;

  bool is_item_shown_ = false;

  global_media_controls::MediaItemUIObserverSet item_ui_observer_set_;

  mojo::RemoteSet<global_media_controls::mojom::DevicePickerObserver>
      observers_;

  mojo::ReceiverSet<global_media_controls::mojom::DevicePickerProvider>
      receivers_;

  base::WeakPtrFactory<SupplementalDevicePickerProducer> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_SUPPLEMENTAL_DEVICE_PICKER_PRODUCER_H_
