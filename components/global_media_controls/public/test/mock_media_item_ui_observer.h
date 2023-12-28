// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_OBSERVER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_OBSERVER_H_

#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace global_media_controls {
namespace test {

class MockMediaItemUIObserver : public MediaItemUIObserver {
 public:
  MockMediaItemUIObserver();
  MockMediaItemUIObserver(const MockMediaItemUIObserver&) = delete;
  MockMediaItemUIObserver& operator=(const MockMediaItemUIObserver&) = delete;
  ~MockMediaItemUIObserver() override;

  MOCK_METHOD(void, OnMediaItemUISizeChanged, ());
  MOCK_METHOD(void, OnMediaItemUIMetadataChanged, ());
  MOCK_METHOD(void, OnMediaItemUIActionsChanged, ());
  MOCK_METHOD(void, OnMediaItemUIClicked, (const std::string&, bool));
  MOCK_METHOD(void, OnMediaItemUIDismissed, (const std::string&));
  MOCK_METHOD(void, OnMediaItemUIDestroyed, (const std::string&));
};

}  // namespace test
}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_OBSERVER_H_
