// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_MANAGER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_MANAGER_H_

#include "components/global_media_controls/public/media_item_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace global_media_controls {
namespace test {

class MockMediaItemManager : public MediaItemManager {
 public:
  MockMediaItemManager();
  MockMediaItemManager(const MockMediaItemManager&) = delete;
  MockMediaItemManager& operator=(const MockMediaItemManager&) = delete;
  ~MockMediaItemManager() override;

  base::WeakPtr<MediaItemManager> GetWeakPtr() override;

  MOCK_METHOD(void, AddObserver, (MediaItemManagerObserver*));
  MOCK_METHOD(void, RemoveObserver, (MediaItemManagerObserver*));
  MOCK_METHOD(void, AddItemProducer, (MediaItemProducer*));
  MOCK_METHOD(void, RemoveItemProducer, (MediaItemProducer*));
  MOCK_METHOD(void, ShowItem, (const std::string&));
  MOCK_METHOD(void, HideItem, (const std::string&));
  MOCK_METHOD(void, RefreshItem, (const std::string&));
  MOCK_METHOD(void, OnItemsChanged, ());
  MOCK_METHOD(void, SetDialogDelegate, (MediaDialogDelegate*));
  MOCK_METHOD(void,
              SetDialogDelegateForId,
              (MediaDialogDelegate*, const std::string&));
  MOCK_METHOD(void, FocusDialog, ());
  MOCK_METHOD(void, HideDialog, ());
  MOCK_METHOD(bool, HasActiveItems, ());
  MOCK_METHOD(bool, HasFrozenItems, ());
  MOCK_METHOD(bool, HasOpenDialog, ());
  MOCK_METHOD(std::list<std::string>, GetActiveItemIds, ());

 private:
  base::WeakPtrFactory<MediaItemManager> weak_ptr_factory_{this};
};

}  // namespace test
}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_MANAGER_H_
