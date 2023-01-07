// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_MANAGER_OBSERVER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_MANAGER_OBSERVER_H_

#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace global_media_controls {
namespace test {

class MockMediaItemManagerObserver : public MediaItemManagerObserver {
 public:
  MockMediaItemManagerObserver();
  MockMediaItemManagerObserver(const MockMediaItemManagerObserver&) = delete;
  MockMediaItemManagerObserver& operator=(const MockMediaItemManagerObserver&) =
      delete;
  ~MockMediaItemManagerObserver() override;

  MOCK_METHOD(void, OnItemListChanged, ());
  MOCK_METHOD(void, OnMediaDialogOpened, ());
  MOCK_METHOD(void, OnMediaDialogClosed, ());
};

}  // namespace test
}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_MANAGER_OBSERVER_H_
