// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_MANAGER_IMPL_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_MANAGER_IMPL_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/media_item_manager.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace global_media_controls {

class MediaDialogDelegate;
class MediaItemManagerObserver;
class MediaItemProducer;

class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemManagerImpl
    : public MediaItemManager {
 public:
  MediaItemManagerImpl();
  MediaItemManagerImpl(const MediaItemManagerImpl&) = delete;
  MediaItemManagerImpl& operator=(const MediaItemManagerImpl&) = delete;
  ~MediaItemManagerImpl() override;

  // MediaItemManager:
  void AddObserver(MediaItemManagerObserver* observer) override;
  void RemoveObserver(MediaItemManagerObserver* observer) override;
  void AddItemProducer(MediaItemProducer* producer) override;
  void RemoveItemProducer(MediaItemProducer* producer) override;
  void ShowItem(const std::string& id) override;
  void HideItem(const std::string& id) override;
  void RefreshItem(const std::string& id) override;
  void OnItemsChanged() override;
  void SetDialogDelegate(MediaDialogDelegate* delegate) override;
  void SetDialogDelegateForId(MediaDialogDelegate* delegate,
                              const std::string& id) override;
  void FocusDialog() override;
  void HideDialog() override;
  bool HasActiveItems() override;
  bool HasFrozenItems() override;
  bool HasOpenDialog() override;
  std::list<std::string> GetActiveItemIds() override;
  base::WeakPtr<MediaItemManager> GetWeakPtr() override;

 private:
  // Finds and shows the media item UI for the given id in an existing dialog,
  // and returns whether it is shown.
  bool ShowMediaItemUI(const std::string& id);

  // Looks up an item from any source.  Returns null if not found.
  base::WeakPtr<media_message_center::MediaNotificationItem> GetItem(
      const std::string& id);

  MediaItemProducer* GetItemProducer(const std::string& item_id);

  // Updates |dialog_delegate_| and notifies |observers_|. Called from
  // SetDialogDelegate() and SetDialogDelegateForId().
  void SetDialogDelegateCommon(MediaDialogDelegate* delegate);

  // True if there is an open MediaDialogView and the dialog is opened for a
  // single item.
  bool HasOpenDialogForItem();

  raw_ptr<MediaDialogDelegate> dialog_delegate_ = nullptr;

  // True if the dialog was opened by |SetDialogDelegateForId()|. The
  // value does not indicate whether the MediaDialogView is opened or not.
  bool dialog_opened_for_single_item_ = false;

  // Pointers to all item producers used by |this|.
  base::flat_set<raw_ptr<MediaItemProducer, CtnExperimental>> item_producers_;

  base::ObserverList<MediaItemManagerObserver> observers_;

  base::WeakPtrFactory<MediaItemManager> weak_ptr_factory_{this};
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_MANAGER_IMPL_H_
