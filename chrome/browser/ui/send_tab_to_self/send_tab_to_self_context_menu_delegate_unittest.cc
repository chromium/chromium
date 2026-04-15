// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_context_menu_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

using testing::ElementsAre;
using testing::Field;

// A stub sync service that simply returns our stub model.
class StubSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  StubSendTabToSelfSyncService() = default;
  ~StubSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override { return &model_; }
  FakeSendTabToSelfModel* GetModelFake() { return &model_; }

 private:
  FakeSendTabToSelfModel model_;
};

class SendTabToSelfContextMenuDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SendTabToSelfContextMenuDelegateTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            &SendTabToSelfContextMenuDelegateTest::BuildStubSyncService,
            base::Unretained(this)));
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  std::unique_ptr<KeyedService> BuildStubSyncService(
      content::BrowserContext* context) {
    return std::make_unique<StubSendTabToSelfSyncService>();
  }

  FakeSendTabToSelfModel* model() {
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(profile()))
        ->GetModelFake();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the delegate correctly truncates the device list to a maximum of 5
// devices.
TEST_F(SendTabToSelfContextMenuDelegateTest, GetDevicesForDisplayLimitsToFive) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  for (int i = 0; i < 10; ++i) {
    devices.emplace_back("Device " + base::NumberToString(i),
                         "guid" + base::NumberToString(i),
                         syncer::DeviceInfo::FormFactor::kDesktop, now);
  }
  model()->SetTargetDeviceInfoSortedList(devices);

  SendTabToSelfContextMenuDelegate delegate(web_contents());
  ui::SimpleMenuModel menu_model(&delegate);
  delegate.PopulateSubmenu(&menu_model);

  // The delegate should return exactly 5 devices + separator + manage item.
  EXPECT_EQ(menu_model.GetItemCount(), 7u);
  EXPECT_EQ(menu_model.GetCommandIdAt(0),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1);
  EXPECT_EQ(menu_model.GetCommandIdAt(4),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST);
}

// Tests that ExecuteCommand correctly triggers the underlying send operation
// with the expected device information.
TEST_F(SendTabToSelfContextMenuDelegateTest, ExecuteCommandSendsToDevice) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0",
                       syncer::DeviceInfo::FormFactor::kDesktop, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  const GURL kExampleUrl("https://example.com");
  const std::u16string kExampleTitle = u"Example Title";
  NavigateAndCommit(kExampleUrl);
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  web_contents()->UpdateTitleForEntry(entry, kExampleTitle);

  SendTabToSelfContextMenuDelegate delegate(web_contents());
  ui::SimpleMenuModel menu_model(&delegate);
  delegate.PopulateSubmenu(&menu_model);

  delegate.ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  std::vector<std::string> guids = model()->GetAllGuids();
  ASSERT_EQ(guids.size(), 1u);
  const SendTabToSelfEntry* sent_entry = model()->GetEntryByGUID(guids[0]);
  EXPECT_EQ(sent_entry->GetTargetDeviceSyncCacheGuid(), "guid0");
  EXPECT_EQ(sent_entry->GetURL(), kExampleUrl);
  EXPECT_EQ(sent_entry->GetTitle(), base::UTF16ToUTF8(kExampleTitle));
}

// Tests that PopulateSubmenu correctly adds the device items and the "Manage
// Devices" item to the menu model.
TEST_F(SendTabToSelfContextMenuDelegateTest,
       PopulateSubmenuAddsDevicesAndManageItem) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0",
                       syncer::DeviceInfo::FormFactor::kDesktop, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  SendTabToSelfContextMenuDelegate delegate(web_contents());
  ui::SimpleMenuModel menu_model(&delegate);
  delegate.PopulateSubmenu(&menu_model);

  // Expect: 1 device item + 1 separator + 1 manage devices item = 3 items.
  ASSERT_EQ(menu_model.GetItemCount(), 3u);
  EXPECT_EQ(menu_model.GetCommandIdAt(0),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1);
  EXPECT_EQ(menu_model.GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(menu_model.GetCommandIdAt(2),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES);
}
}  // namespace

}  // namespace send_tab_to_self
