// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/test/mock_media_item_ui_device_selector.h"

#include "ui/base/metadata/metadata_impl_macros.h"

namespace global_media_controls {
namespace test {

MockMediaItemUIDeviceSelector::MockMediaItemUIDeviceSelector() = default;

MockMediaItemUIDeviceSelector::~MockMediaItemUIDeviceSelector() {
  Die();
}

BEGIN_METADATA(MockMediaItemUIDeviceSelector)
END_METADATA

}  // namespace test
}  // namespace global_media_controls
