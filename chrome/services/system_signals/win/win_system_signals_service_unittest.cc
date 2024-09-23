// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/win/win_system_signals_service.h"

#include <array>
#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_os_info_override_win.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/win/win_types.h"
#include "components/device_signals/core/system_signals/file_system_service.h"
#include "components/device_signals/core/system_signals/mock_file_system_service.h"
#include "components/device_signals/core/system_signals/win/mock_wmi_client.h"
#include "components/device_signals/core/system_signals/win/mock_wsc_client.h"
#include "components/device_signals/core/system_signals/win/wmi_client.h"
#include "components/device_signals/core/system_signals/win/wsc_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using device_signals::MockFileSystemService;
using device_signals::MockWmiClient;
using device_signals::MockWscClient;
using testing::Return;

namespace system_signals {

class WinSystemSignalsServiceTest : public testing::Test {
 protected:
  WinSystemSignalsServiceTest() {
    auto file_system_service =
        std::make_unique<testing::StrictMock<MockFileSystemService>>();
    file_system_service_ = file_system_service.get();

    auto wmi_client = std::make_unique<testing::StrictMock<MockWmiClient>>();
    wmi_client_ = wmi_client.get();

    auto wsc_client = std::make_unique<testing::StrictMock<MockWscClient>>();
    wsc_client_ = wsc_client.get();

    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
        fake_receiver;

    // Have to use "new" since make_unique doesn't have access to friend private
    // constructor.
    win_system_signals_service_ =
        std::unique_ptr<WinSystemSignalsService>(new WinSystemSignalsService(
            std::move(fake_receiver), std::move(file_system_service),
            std::move(wmi_client), std::move(wsc_client)));
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::optional<base::test::ScopedOSInfoOverride> os_info_override_;

  std::unique_ptr<WinSystemSignalsService> win_system_signals_service_;

  // Owned by win_system_signals_service_.
  raw_ptr<MockFileSystemService> file_system_service_;
  raw_ptr<MockWmiClient> wmi_client_;
  raw_ptr<MockWscClient> wsc_client_;
};

// Tests that GetFileSystemSignals forwards the signal collection to
// FileSystemService.
TEST_F(WinSystemSignalsServiceTest, GetFileSystemSignals) {
  device_signals::GetFileSystemInfoOptions options;
  options.file_path = base::FilePath::FromUTF8Unsafe("/some/file/path");

  std::vector<device_signals::GetFileSystemInfoOptions> requests;
  requests.push_back(std::move(options));

  device_signals::FileSystemItem returned_item;
  returned_item.file_path =
      base::FilePath::FromUTF8Unsafe("/some/other/file/path");
  returned_item.presence = device_signals::PresenceValue::kFound;

  std::vector<device_signals::FileSystemItem> response;
  response.push_back(std::move(returned_item));

  EXPECT_CALL(*file_system_service_, GetSignals(requests))
      .WillOnce(Return(response));

  base::test::TestFuture<const std::vector<device_signals::FileSystemItem>&>
      future;
  win_system_signals_service_->GetFileSystemSignals(requests,
                                                    future.GetCallback());

  auto results = future.Get();
  EXPECT_EQ(results.size(), response.size());
  EXPECT_EQ(results[0], response[0]);
}

// Tests that AV products cannot be retrieved on Win Server environments.
TEST_F(WinSystemSignalsServiceTest, GetAntiVirusSignals_Server) {
  std::array<base::test::ScopedOSInfoOverride::Type, 2> server_versions = {
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

// Tests that AV products are retrieved through WSC on Win10 and above.
TEST_F(WinSystemSignalsServiceTest, GetAntiVirusSignals_Wsc_Success) {
  std::array<base::test::ScopedOSInfoOverride::Type, 4> win_versions = {
      base::test::ScopedOSInfoOverride::Type::kWin10Pro,
      base::test::ScopedOSInfoOverride::Type::kWin10Pro21H1,
      base::test::ScopedOSInfoOverride::Type::kWin11Home,
      base::test::ScopedOSInfoOverride::Type::kWin11Pro,
  };

  int counter = 0;
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

    histogram_tester_.ExpectUniqueSample(
        "Enterprise.SystemSignals.Collection.WSC.AntiVirus.ParsingError.Rate",
        /*error_rate=*/0, ++counter);
  }
}

// Tests the behavior when WSC returns no items nor error.
TEST_F(WinSystemSignalsServiceTest, GetAntiVirusSignals_Wsc_Empty) {
  os_info_override_.emplace(base::test::ScopedOSInfoOverride::Type::kWin10Pro);

  device_signals::WscAvProductsResponse fake_response;

  EXPECT_CALL(*wsc_client_, GetAntiVirusProducts())
      .WillOnce(Return(fake_response));

  base::test::TestFuture<const std::vector<device_signals::AvProduct>&> future;
  win_system_signals_service_->GetAntiVirusSignals(future.GetCallback());

  const auto& av_products = future.Get();
  EXPECT_TRUE(av_products.empty());

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.SystemSignals.Collection.WSC.AntiVirus.ParsingError.Rate",
      /*error_rate=*/0, 1);
}

// Tests the behavior when a Query error is returned from querying WSC.
TEST_F(WinSystemSignalsServiceTest, GetAntiVirusSignals_Wsc_QueryError) {
  os_info_override_.emplace(base::test::ScopedOSInfoOverride::Type::kWin10Pro);

  device_signals::WscAvProductsResponse fake_response;
  fake_response.query_error =
      device_signals::WscQueryError::kFailedToCreateInstance;

  EXPECT_CALL(*wsc_client_, GetAntiVirusProducts())
      .WillOnce(Return(fake_response));

  base::test::TestFuture<const std::vector<device_signals::AvProduct>&> future;
  win_system_signals_service_->GetAntiVirusSignals(future.GetCallback());

  const auto& av_products = future.Get();
  EXPECT_TRUE(av_products.empty());

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.SystemSignals.Collection.WSC.AntiVirus.QueryError",
      fake_response.query_error.value(), 1);
}

// Tests the behavior when parsing errors are returned from querying WSC.
TEST_F(WinSystemSignalsServiceTest, GetAntiVirusSignals_Wsc_MixedParsingError) {
  os_info_override_.emplace(base::test::ScopedOSInfoOverride::Type::kWin10Pro);
  device_signals::AvProduct fake_av_product;
  fake_av_product.display_name = "some display name";
  fake_av_product.product_id = "some product id";
  fake_av_product.state = device_signals::AvProductState::kOn;

  // Adding 2 success and 2 failures, so the error rate should be 50%.
  device_signals::WscAvProductsResponse fake_response;
  fake_response.av_products.push_back(fake_av_product);
  fake_response.av_products.push_back(fake_av_product);
  fake_response.parsing_errors.push_back(
      device_signals::WscParsingError::kFailedToGetState);
  fake_response.parsing_errors.push_back(
      device_signals::WscParsingError::kStateInvalid);

  EXPECT_CALL(*wsc_client_, GetAntiVirusProducts())
      .WillOnce(Return(fake_response));

  base::test::TestFuture<const std::vector<device_signals::AvProduct>&> future;
  win_system_signals_service_->GetAntiVirusSignals(future.GetCallback());

  const auto& av_products = future.Get();
  EXPECT_EQ(av_products.size(), fake_response.av_products.size());

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SystemSignals.Collection.WSC.AntiVirus.ParsingError",
      device_signals::WscParsingError::kFailedToGetState, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SystemSignals.Collection.WSC.AntiVirus.ParsingError",
      device_signals::WscParsingError::kStateInvalid, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SystemSignals.Collection.WSC.AntiVirus.ParsingError.Rate",
      /*error_rate=*/50, 1);
}

// Tests that Hotfix information is retrieved via WMI.
TEST_F(WinSystemSignalsServiceTest, GetHotfixSignals_Empty) {
  device_signals::InstalledHotfix fake_hotfix{"some hotfix id"};
  device_signals::WmiHotfixesResponse fake_response;

  EXPECT_CALL(*wmi_client_, GetInstalledHotfixes())
      .WillOnce(Return(fake_response));

  base::test::TestFuture<const std::vector<device_signals::InstalledHotfix>&>
      future;
  win_system_signals_service_->GetHotfixSignals(future.GetCallback());

  const auto& hotfixes_response = future.Get();
  EXPECT_TRUE(hotfixes_response.empty());

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.SystemSignals.Collection.WMI.Hotfixes.ParsingError.Rate",
      /*error_rate=*/0, 1);
}

// Tests that Hotfix information is retrieved via WMI.
TEST_F(WinSystemSignalsServiceTest, GetHotfixSignals_Success) {
  device_signals::InstalledHotfix fake_hotfix{"some hotfix id"};
  device_signals::WmiHotfixesResponse fake_response;
  fake_response.hotfixes.push_back(fake_hotfix);

  EXPECT_CALL(*wmi_client_, GetInstalledHotfixes())
      .WillOnce(Return(fake_response));

  base::test::TestFuture<const std::vector<device_signals::InstalledHotfix>&>
      future;
  win_system_signals_service_->GetHotfixSignals(future.GetCallback());

  const auto& hotfixes_response = future.Get();
  EXPECT_EQ(hotfixes_response.size(), fake_response.hotfixes.size());
  EXPECT_EQ(hotfixes_response[0].hotfix_id,
            fake_response.hotfixes[0].hotfix_id);

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.SystemSignals.Collection.WMI.Hotfixes.ParsingError.Rate",
      /*error_rate=*/0, 1);
}

// Tests that a query error is returned when querying Hotfixes via WMI.
TEST_F(WinSystemSignalsServiceTest, GetHotfixSignals_QueryError) {
  device_signals::WmiHotfixesResponse fake_response;
  fake_response.query_error = base::win::WmiError::kFailedToCreateInstance;

  EXPECT_CALL(*wmi_client_, GetInstalledHotfixes())
      .WillOnce(Return(fake_response));

  base::test::TestFuture<const std::vector<device_signals::InstalledHotfix>&>
      future;
  win_system_signals_service_->GetHotfixSignals(future.GetCallback());

  const auto& hotfixes_response = future.Get();
  EXPECT_TRUE(hotfixes_response.empty());

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.SystemSignals.Collection.WMI.Hotfixes.QueryError",
      fake_response.query_error.value(), 1);
}

// Tests that items and parsing errors are returned when querying Hotfixes via
// WMI.
TEST_F(WinSystemSignalsServiceTest, GetHotfixSignals_MixedParsingErrors) {
  device_signals::InstalledHotfix fake_hotfix{"some hotfix id"};

  // Adding 2 success and 2 failures, so the error rate should be 50%.
  device_signals::WmiHotfixesResponse fake_response;
  fake_response.hotfixes.push_back(fake_hotfix);
  fake_response.hotfixes.push_back(fake_hotfix);
  fake_response.parsing_errors.push_back(
      device_signals::WmiParsingError::kFailedToGetState);
  fake_response.parsing_errors.push_back(
      device_signals::WmiParsingError::kFailedToGetId);

  EXPECT_CALL(*wmi_client_, GetInstalledHotfixes())
      .WillOnce(Return(fake_response));

  base::test::TestFuture<const std::vector<device_signals::InstalledHotfix>&>
      future;
  win_system_signals_service_->GetHotfixSignals(future.GetCallback());

  const auto& hotfixes_response = future.Get();
  EXPECT_EQ(hotfixes_response.size(), fake_response.hotfixes.size());

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SystemSignals.Collection.WMI.Hotfixes.ParsingError",
      device_signals::WmiParsingError::kFailedToGetState, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SystemSignals.Collection.WMI.Hotfixes.ParsingError",
      device_signals::WmiParsingError::kFailedToGetId, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SystemSignals.Collection.WMI.Hotfixes.ParsingError.Rate",
      /*error_rate=*/50, 1);
}

}  // namespace system_signals
