// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_producer.h"

#include <utility>

#include "components/global_media_controls/public/media_item_manager.h"

SupplementalDevicePickerProducer::SupplementalDevicePickerProducer(
    global_media_controls::MediaItemManager* item_manager)
    : item_manager_(item_manager), item_ui_observer_set_(this) {
  item_manager_->AddObserver(this);
}

SupplementalDevicePickerProducer::~SupplementalDevicePickerProducer() {
  item_manager_->RemoveObserver(this);
}

base::WeakPtr<media_message_center::MediaNotificationItem>
SupplementalDevicePickerProducer::GetMediaItem(const std::string& id) {
  if (item_ && item_->id() == id) {
    return item_->GetWeakPtr();
  } else {
    return nullptr;
  }
}

std::set<std::string>
SupplementalDevicePickerProducer::GetActiveControllableItemIds() const {
  return (item_ && is_item_shown_) ? std::set<std::string>({item_->id()})
                                   : std::set<std::string>();
}

bool SupplementalDevicePickerProducer::HasFrozenItems() {
  return false;
}

void SupplementalDevicePickerProducer::OnItemShown(
    const std::string& id,
    global_media_controls::MediaItemUI* item_ui) {
  if (item_ui) {
    item_ui_observer_set_.Observe(id, item_ui);
  }
}

bool SupplementalDevicePickerProducer::IsItemActivelyPlaying(
    const std::string& id) {
  return false;
}

void SupplementalDevicePickerProducer::OnMediaItemUIDismissed(
    const std::string& id) {
  auto item = GetMediaItem(id);
  if (item) {
    item->Dismiss();
    for (auto& observer : observers_) {
      observer->OnPickerDismissed();
    }
  }
}

void SupplementalDevicePickerProducer::CreateItem(
    const base::UnguessableToken& source_id) {
  if (!item_) {
    item_.emplace(item_manager_, source_id);
  }
}

void SupplementalDevicePickerProducer::DeleteItem() {
  if (!item_) {
    return;
  }
  const auto id{item_->id()};
  item_.reset();
  item_manager_->HideItem(id);
  is_item_shown_ = false;
}

void SupplementalDevicePickerProducer::ShowItem() {
  if (!item_) {
    return;
  }
  if (!is_item_shown_) {
    is_item_shown_ = true;
    item_manager_->ShowItem(item_->id());
  }
}

void SupplementalDevicePickerProducer::HideItem() {
  if (!item_) {
    return;
  }
  item_manager_->HideItem(item_->id());
  is_item_shown_ = false;
}

void SupplementalDevicePickerProducer::OnMetadataChanged(
    const media_session::MediaMetadata& metadata) {
  if (!item_) {
    return;
  }
  item_->UpdateViewWithMetadata(metadata);
}

void SupplementalDevicePickerProducer::OnArtworkImageChanged(
    const gfx::ImageSkia& artwork_image) {
  if (!item_) {
    return;
  }
  item_->UpdateViewWithArtworkImage(artwork_image);
}

void SupplementalDevicePickerProducer::OnFaviconImageChanged(
    const gfx::ImageSkia& favicon_image) {
  if (!item_) {
    return;
  }
  item_->UpdateViewWithFaviconImage(favicon_image);
}

void SupplementalDevicePickerProducer::AddObserver(
    mojo::PendingRemote<global_media_controls::mojom::DevicePickerObserver>
        observer) {
  observers_.Add(std::move(observer));
}

void SupplementalDevicePickerProducer::HideMediaUI() {
  item_manager_->HideDialog();
}

const SupplementalDevicePickerItem&
SupplementalDevicePickerProducer::GetOrCreateNotificationItem(
    const base::UnguessableToken& source_id) {
  CreateItem(source_id);
  CHECK(item_);
  return *item_;
}

mojo::PendingRemote<global_media_controls::mojom::DevicePickerProvider>
SupplementalDevicePickerProducer::PassRemote() {
  mojo::PendingReceiver<global_media_controls::mojom::DevicePickerProvider>
      receiver;
  auto pending_remote = receiver.InitWithNewPipeAndPassRemote();
  receivers_.Add(this, std::move(receiver));
  return pending_remote;
}

void SupplementalDevicePickerProducer::OnItemListChanged() {
  for (auto& observer : observers_) {
    observer->OnMediaUIUpdated();
  }
}

void SupplementalDevicePickerProducer::OnMediaDialogOpened() {
  for (auto& observer : observers_) {
    observer->OnMediaUIOpened();
  }
}

void SupplementalDevicePickerProducer::OnMediaDialogClosed() {
  for (auto& observer : observers_) {
    observer->OnMediaUIClosed();
  }
}
