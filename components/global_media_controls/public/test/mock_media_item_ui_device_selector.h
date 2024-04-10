// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_

#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace global_media_controls {
namespace test {

class MockMediaItemUIDeviceSelector : public MediaItemUIDeviceSelector {
  METADATA_HEADER(MockMediaItemUIDeviceSelector, MediaItemUIDeviceSelector)

 public:
  MockMediaItemUIDeviceSelector();
  MockMediaItemUIDeviceSelector(const MockMediaItemUIDeviceSelector&) = delete;
  MockMediaItemUIDeviceSelector& operator=(
      const MockMediaItemUIDeviceSelector&) = delete;
  ~MockMediaItemUIDeviceSelector() override;

  // MediaItemUIDeviceSelector:
  MOCK_METHOD(void, SetMediaItemUIView, (MediaItemUIView*));
  MOCK_METHOD(void, SetMediaItemUIUpdatedView, (MediaItemUIUpdatedView*));
  MOCK_METHOD(void, OnColorsChanged, (SkColor, SkColor));
  MOCK_METHOD(void, UpdateCurrentAudioDevice, (const std::string&));
  MOCK_METHOD(void, ShowDevices, ());
  MOCK_METHOD(void, HideDevices, ());
  MOCK_METHOD(bool, IsDeviceSelectorExpanded, ());

  MOCK_METHOD(void, Die, ());
};

}  // namespace test
}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_
