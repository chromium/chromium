// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_MOCK_WSC_CLIENT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_MOCK_WSC_CLIENT_H_

#include "components/device_signals/core/system_signals/win/wsc_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockWscClient : public WscClient {
 public:
  MockWscClient();
  ~MockWscClient() override;

  MOCK_METHOD(WscAvProductsResponse, GetAntiVirusProducts, (), (override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_MOCK_WSC_CLIENT_H_
