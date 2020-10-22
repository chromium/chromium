// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace send_tab_to_self {

namespace {

using testing::_;
using testing::Return;
using testing::SaveArg;

class SendTabToSelfModelMock : public TestSendTabToSelfModel {
 public:
  SendTabToSelfModelMock() = default;
  ~SendTabToSelfModelMock() override = default;

  MOCK_METHOD0(GetTargetDeviceInfoSortedList, std::vector<TargetDeviceInfo>());

  MOCK_METHOD4(AddEntry,
               const SendTabToSelfEntry*(const GURL&,
                                         const std::string&,
                                         base::Time,
                                         const std::string&));

  bool IsReady() override { return true; }
};

class TestSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService() = default;
  ~TestSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override {
    return &send_tab_to_self_model_mock_;
  }

 protected:
  SendTabToSelfModelMock send_tab_to_self_model_mock_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSendTabToSelfSyncService>();
}

TargetDeviceInfo BuildTargetDeviceInfo(const std::string& device_name,
                                       const std::string& cache_guid) {
  return TargetDeviceInfo(device_name, device_name, cache_guid,
                          sync_pb::SyncEnums_DeviceType_TYPE_OTHER,
                          base::Time());
}

class SendTabToSelfSubMenuModelTest : public BrowserWithTestWindowTest {
 public:
  SendTabToSelfSubMenuModelTest() = default;
  ~SendTabToSelfSubMenuModelTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Set up all test conditions to let ShouldOfferFeature() return true.
    GURL url("https://www.test.com");
    AddTab(browser(), url);
    NavigateAndCommitActiveTabWithTitle(browser(), url,
                                        base::ASCIIToUTF16("test"));
  }

  void SetUpTestService() {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&BuildTestSendTabToSelfSyncService));
  }

  SendTabToSelfModelMock* GetSendTabToSelfModelMock() {
    return static_cast<SendTabToSelfModelMock*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(browser()->profile())
            ->GetSendTabToSelfModel());
  }
};

TEST_F(SendTabToSelfSubMenuModelTest, ExecuteCommandTab) {
  SetUpTestService();

  SendTabToSelfModelMock* model_mock = GetSendTabToSelfModelMock();
  std::vector<TargetDeviceInfo> devices = {
      BuildTargetDeviceInfo("device0", "0"),
      BuildTargetDeviceInfo("device1", "1"),
      BuildTargetDeviceInfo("device2", "2")};

  EXPECT_CALL(*model_mock, GetTargetDeviceInfoSortedList())
      .WillOnce(Return(devices));
  SendTabToSelfSubMenuModel sub_menu_model(
      browser()->tab_strip_model()->GetActiveWebContents(),
      send_tab_to_self::SendTabToSelfMenuType::kTab);

  std::string device_guid;
  EXPECT_CALL(*model_mock, AddEntry(_, _, _, _))
      .WillRepeatedly(
          DoAll(SaveArg<3>(&device_guid), testing::Return(nullptr)));

  // Check that all devices can be selected.
  for (int i = 0; i < (int)devices.size(); i++) {
    device_guid = std::string();
    sub_menu_model.ExecuteCommand(SendTabToSelfSubMenuModel::kMinCommandId + i,
                                  -1);
    EXPECT_EQ(devices[i].cache_guid, device_guid) << "for index: " << i;
  }
}

}  // namespace

}  // namespace send_tab_to_self
