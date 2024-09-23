// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/media_item_manager_impl.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"

namespace global_media_controls {

// static
std::unique_ptr<MediaItemManager> MediaItemManager::Create() {
  return std::make_unique<MediaItemManagerImpl>();
}

MediaItemManagerImpl::MediaItemManagerImpl() = default;

MediaItemManagerImpl::~MediaItemManagerImpl() = default;

void MediaItemManagerImpl::AddObserver(MediaItemManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaItemManagerImpl::RemoveObserver(MediaItemManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaItemManagerImpl::AddItemProducer(MediaItemProducer* producer) {
  item_producers_.insert(producer);
}

void MediaItemManagerImpl::RemoveItemProducer(MediaItemProducer* producer) {
  item_producers_.erase(producer);
}

void MediaItemManagerImpl::ShowItem(const std::string& id) {
  // If new items come up while the dialog is open for a particular item, do not
  // show the new items.
  if (!HasOpenDialogForItem()) {
    OnItemsChanged();
    if (!dialog_delegate_) {
      return;
    }
    ShowMediaItemUI(id);
  }
}

void MediaItemManagerImpl::HideItem(const std::string& id) {
  OnItemsChanged();
  if (!dialog_delegate_) {
    return;
  }
  dialog_delegate_->HideMediaItem(id);
}

void MediaItemManagerImpl::RefreshItem(const std::string& id) {
  if (!dialog_delegate_) {
    return;
  }
  dialog_delegate_->RefreshMediaItem(id, GetItem(id));
}

void MediaItemManagerImpl::OnItemsChanged() {
  for (auto& observer : observers_) {
    observer.OnItemListChanged();
  }
}

void MediaItemManagerImpl::SetDialogDelegate(MediaDialogDelegate* delegate) {
  dialog_opened_for_single_item_ = false;
  SetDialogDelegateCommon(delegate);
  if (!dialog_delegate_) {
    return;
  }
  auto item_ids = GetActiveItemIds();
  for (const std::string& id : item_ids) {
    ShowMediaItemUI(id);
  }
  for (MediaItemProducer* item_producer : item_producers_) {
    item_producer->OnDialogDisplayed();
  }
  media_message_center::RecordConcurrentNotificationCount(item_ids.size());
}

void MediaItemManagerImpl::SetDialogDelegateForId(MediaDialogDelegate* delegate,
                                                  const std::string& id) {
  SetDialogDelegateCommon(delegate);
  if (!dialog_delegate_) {
    return;
  }
  if (ShowMediaItemUI(id)) {
    dialog_opened_for_single_item_ = true;
  }
}

void MediaItemManagerImpl::FocusDialog() {
  if (dialog_delegate_) {
    dialog_delegate_->Focus();
  }
}

void MediaItemManagerImpl::HideDialog() {
  if (dialog_delegate_) {
    dialog_delegate_->HideMediaDialog();
  }
}

bool MediaItemManagerImpl::HasActiveItems() {
  return !GetActiveItemIds().empty();
}

bool MediaItemManagerImpl::HasFrozenItems() {
  for (MediaItemProducer* item_producer : item_producers_) {
    if (item_producer->HasFrozenItems()) {
      return true;
    }
  }
  return false;
}

bool MediaItemManagerImpl::HasOpenDialog() {
  return !!dialog_delegate_;
}

std::list<std::string> MediaItemManagerImpl::GetActiveItemIds() {
  // Get all the active item IDs and remove duplicates.
  std::set<std::string> item_ids;
  for (MediaItemProducer* item_producer : item_producers_) {
    const std::set<std::string>& ids =
        item_producer->GetActiveControllableItemIds();
    item_ids.insert(ids.begin(), ids.end());
  }

  // Sort the item IDs.
  std::list<std::string> sorted_item_ids;
  for (const std::string& id : item_ids) {
    auto* item_producer = GetItemProducer(id);
    DCHECK(item_producer);

    if (item_producer->IsItemActivelyPlaying(id)) {
      sorted_item_ids.push_front(id);
    } else {
      sorted_item_ids.push_back(id);
    }
  }
  return sorted_item_ids;
}

bool MediaItemManagerImpl::ShowMediaItemUI(const std::string& id) {
  base::WeakPtr<media_message_center::MediaNotificationItem> item = GetItem(id);
  if (!item) {
    return false;
  }
  CHECK(dialog_delegate_);
  MediaItemUI* item_ui = dialog_delegate_->ShowMediaItem(id, item);
  MediaItemProducer* producer = GetItemProducer(id);
  CHECK(producer);
  producer->OnItemShown(id, item_ui);
  base::UmaHistogramEnumeration(
      media_message_center::MediaNotificationItem::kSourceHistogramName,
      item->GetSource());
  return true;
}

base::WeakPtr<media_message_center::MediaNotificationItem>
MediaItemManagerImpl::GetItem(const std::string& id) {
  for (MediaItemProducer* producer : item_producers_) {
    auto item = producer->GetMediaItem(id);
    if (item) {
      return item;
    }
  }
  return nullptr;
}

MediaItemProducer* MediaItemManagerImpl::GetItemProducer(
    const std::string& item_id) {
  for (MediaItemProducer* producer : item_producers_) {
    if (producer->GetMediaItem(item_id)) {
      return producer;
    }
  }
  return nullptr;
}

void MediaItemManagerImpl::SetDialogDelegateCommon(
    MediaDialogDelegate* delegate) {
  DCHECK(!delegate || !dialog_delegate_);
  dialog_delegate_ = delegate;

  if (dialog_delegate_) {
    for (auto& observer : observers_) {
      observer.OnMediaDialogOpened();
    }
  } else {
    for (auto& observer : observers_) {
      observer.OnMediaDialogClosed();
    }
  }
}

bool MediaItemManagerImpl::HasOpenDialogForItem() {
  return HasOpenDialog() && dialog_opened_for_single_item_;
}

base::WeakPtr<MediaItemManager> MediaItemManagerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace global_media_controls
