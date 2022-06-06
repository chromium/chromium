// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/win/win_system_signals_service.h"

#include <array>
#include <memory>

#include "base/test/scoped_os_info_override_win.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/common/win/mock_wmi_client.h"
#include "components/device_signals/core/common/win/mock_wsc_client.h"
#include "components/device_signals/core/common/win/win_types.h"
#include "components/device_signals/core/common/win/wmi_client.h"
#include "components/device_signals/core/common/win/wsc_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using device_signals::MockWmiClient;
using device_signals::MockWscClient;
using testing::Return;

namespace system_signals {

class WinSystemSignalsServiceTest : public testing::Test {
 protected:
  WinSystemSignalsServiceTest() {
    auto wmi_client = std::make_unique<testing::StrictMock<MockWmiClient>>();
    wmi_client_ = wmi_client.get();

    auto wsc_client = std::make_unique<testing::StrictMock<MockWscClient>>();
    wsc_client_ = wsc_client.get();

    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
        fake_receiver;
    win_system_signals_service_ =
        std::unique_ptr<WinSystemSignalsService>(new WinSystemSignalsService(
            std::move(fake_receiver), std::move(wmi_client),
            std::move(wsc_client)));
  }

  base::test::TaskEnvironment task_environment_;
  absl::optional<base::test::ScopedOSInfoOverride> os_info_override_;

  MockWmiClient* wmi_client_;
  MockWscClient* wsc_client_;
  std::unique_ptr<WinSystemSignalsService> win_system_signals_service_;
};

// Tests that AV products cannot be retrieve on Win Server environments.
TEST_F(WinSystemSignalsServiceTest, GetAntiVirusSignals_Server) {
  std::array<base::test::ScopedOSInfoOverride::Type, 3> server_versions = {
      base::test::ScopedOSInfoOverride::Type::kWinServer2012R2,
      base::test::ScopedOSInfoOverride::Type::kWinServer2016,
      base::test::ScopedOSInfoOverride::Type::kWinServer2022,
  };

  for (const auto server_version : server_versions) {
    os_info_override_.emplace(server_version);

    base::test::TestFuture<const std::vector<device_signals::AvProduct>&>
        future;
    win_system_signals_service_->GetAntiVirusSignals(future.GetCallback());

    EXPECT_EQ(future.Get().size(), 0U);
  }
}

// Tests that AV products are retrieved through WSC on Win8 and above.
TEST_F(WinSystemSignalsServiceTest, GetAntiVirusSignals_Wsc_Success) {
  std::array<base::test::ScopedOSInfoOverride::Type, 5> win_versions = {
      base::test::ScopedOSInfoOverride::Type::kWin81Pro,
      base::test::ScopedOSInfoOverride::Type::kWin10Pro,
      base::test::ScopedOSInfoOverride::Type::kWin10Pro21H1,
      base::test::ScopedOSInfoOverride::Type::kWin11Home,
      base::test::ScopedOSInfoOverride::Type::kWin11Pro,
  };

  for (const auto win_version : win_versions) {
    os_info_override_.emplace(win_version);

    device_signals::AvProduct fake_av_product;
    fake_av_product.display_name = "some display name";
    fake_av_product.product_id = "some product id";
    fake_av_product.state = device_signals::AvProductState::kOn;

    device_signals::WscAvProductsResponse fake_response;
    fake_response.av_products.push_back(fake_av_product);

    EXPECT_CALL(*wsc_client_, GetAntiVirusProducts())
        .WillOnce(Return(fake_response));

    base::test::TestFuture<const std::vector<device_signals::AvProduct>&>
        future;
    win_system_signals_service_->GetAntiVirusSignals(future.GetCallback());

    const auto& av_products = future.Get();
    EXPECT_EQ(av_products.size(), fake_response.av_products.size());
    EXPECT_EQ(av_products[0].product_id,
              fake_response.av_products[0].product_id);
  }
}

// Tests that AV products are not retrieved on Win7.
TEST_F(WinSystemSignalsServiceTest, GetAntiVirusSignals_Win7) {
  os_info_override_.emplace(
      base::test::ScopedOSInfoOverride::Type::kWin7ProSP1);

  base::test::TestFuture<const std::vector<device_signals::AvProduct>&> future;
  win_system_signals_service_->GetAntiVirusSignals(future.GetCallback());

  EXPECT_EQ(future.Get().size(), 0U);
}

}  // namespace system_signals
