// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_MANAGER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_MANAGER_H_

#include <list>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"

namespace global_media_controls {

class MediaDialogDelegate;
class MediaItemManagerObserver;
class MediaItemProducer;

class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemManager {
 public:
  static std::unique_ptr<MediaItemManager> Create();

  virtual ~MediaItemManager() = default;

  virtual void AddObserver(MediaItemManagerObserver* observer) = 0;

  virtual void RemoveObserver(MediaItemManagerObserver* observer) = 0;

  // Adds a MediaItemProducer to the list of producers used by the
  // MediaItemManager to compute the list of active media items.
  virtual void AddItemProducer(MediaItemProducer* producer) = 0;

  // Removes the MediaItemProducer that was added via |AddItemProducer()|.
  virtual void RemoveItemProducer(MediaItemProducer* producer) = 0;

  // The notification with the given id should be shown.
  virtual void ShowItem(const std::string& id) = 0;

  // The notification with the given id should be hidden.
  virtual void HideItem(const std::string& id) = 0;

  // The notification with the given id should be refreshed with new UI.
  virtual void RefreshItem(const std::string& id) = 0;

  // Called by item producers when items have changed.
  virtual void OnItemsChanged() = 0;

  // Populates a newly opened dialog with all active and controllable media
  // items.
  virtual void SetDialogDelegate(MediaDialogDelegate* delegate) = 0;

  // Populates a newly opened dialog with only the given item.
  virtual void SetDialogDelegateForId(MediaDialogDelegate* delegate,
                                      const std::string& id) = 0;

  // Changes focus to the dialog if it exists.
  virtual void FocusDialog() = 0;

  // Hides the open dialog if it exists. No-op otherwise.
  virtual void HideDialog() = 0;

  // True if there are active non-frozen items.
  virtual bool HasActiveItems() = 0;

  // True if there are active frozen items.
  virtual bool HasFrozenItems() = 0;

  // True if there is an open MediaDialogDelegate associated with this service.
  virtual bool HasOpenDialog() = 0;

  // Returns active media item IDs gathered from all the item producers and
  // items being actively played will be in the front.
  virtual std::list<std::string> GetActiveItemIds() = 0;

  virtual base::WeakPtr<MediaItemManager> GetWeakPtr() = 0;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_ITEM_MANAGER_H_
