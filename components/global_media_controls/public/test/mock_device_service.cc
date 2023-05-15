// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/test/mock_device_service.h"

namespace global_media_controls::test {

MockDeviceService::MockDeviceService() = default;
MockDeviceService::~MockDeviceService() = default;

mojo::PendingRemote<mojom::DeviceService> MockDeviceService::PassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockDeviceService::ResetReceiver() {
  receiver_.reset();
}

void MockDeviceService::FlushForTesting() {
  receiver_.FlushForTesting();
}

}  // namespace global_media_controls::test
