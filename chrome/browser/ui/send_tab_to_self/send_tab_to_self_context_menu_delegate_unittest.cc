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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

using testing::ElementsAre;
using testing::Field;
using testing::UnorderedElementsAre;

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
        ->GetFakeSendTabToSelfModel();
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

  SendTabToSelfContextMenuDelegate delegate(web_contents(),
                                            ShareEntryPoint::kContentMenu);
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

  SendTabToSelfContextMenuDelegate delegate(web_contents(),
                                            ShareEntryPoint::kContentMenu);
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

// Tests that ExecuteCommand uses the target URL and target title passed to the
// constructor when sending to a device (e.g., when right-clicking a hyperlink).
TEST_F(SendTabToSelfContextMenuDelegateTest,
       ExecuteCommandSendsTargetUrlAndTitleWhenProvided) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0",
                       syncer::DeviceInfo::FormFactor::kDesktop, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  const GURL kPageUrl("https://example.com/page");
  const GURL kLinkUrl("https://example.com/link");
  const std::string kLinkTitle = "Link Anchor Text";
  NavigateAndCommit(kPageUrl);

  SendTabToSelfContextMenuDelegate delegate(
      web_contents(), ShareEntryPoint::kLinkMenu, kLinkUrl, kLinkTitle);
  ui::SimpleMenuModel menu_model(&delegate);
  delegate.PopulateSubmenu(&menu_model);

  delegate.ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  std::vector<std::string> guids = model()->GetAllGuids();
  ASSERT_EQ(guids.size(), 1u);
  const SendTabToSelfEntry* sent_entry = model()->GetEntryByGUID(guids[0]);
  EXPECT_EQ(sent_entry->GetTargetDeviceSyncCacheGuid(), "guid0");
  EXPECT_EQ(sent_entry->GetURL(), kLinkUrl);
  EXPECT_EQ(sent_entry->GetTitle(), kLinkTitle);
}

// Tests that when target title is empty, the delegate falls back to the parent
// web contents page title during ExecuteCommand.
TEST_F(SendTabToSelfContextMenuDelegateTest,
       ExecuteCommandSendsTitleFallbackWhenTitleEmpty) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0",
                       syncer::DeviceInfo::FormFactor::kDesktop, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  const GURL kPageUrl("https://example.com/page");
  const std::u16string kPageTitle = u"Page Title";
  const GURL kLinkUrl("https://example.com/link");
  NavigateAndCommit(kPageUrl);
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  web_contents()->UpdateTitleForEntry(entry, kPageTitle);

  SendTabToSelfContextMenuDelegate delegate(
      web_contents(), ShareEntryPoint::kLinkMenu, kLinkUrl);
  ui::SimpleMenuModel menu_model(&delegate);
  delegate.PopulateSubmenu(&menu_model);

  delegate.ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  std::vector<std::string> guids = model()->GetAllGuids();
  ASSERT_EQ(guids.size(), 1u);
  const SendTabToSelfEntry* sent_entry = model()->GetEntryByGUID(guids[0]);
  EXPECT_EQ(sent_entry->GetURL(), kLinkUrl);
  EXPECT_EQ(sent_entry->GetTitle(), base::UTF16ToUTF8(kPageTitle));
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

  SendTabToSelfContextMenuDelegate delegate(web_contents(),
                                            ShareEntryPoint::kContentMenu);
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

// Tests that OnMenuWillShow correctly records device count metrics.
TEST_F(SendTabToSelfContextMenuDelegateTest, OnMenuWillShowRecordsMetrics) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0",
                       syncer::DeviceInfo::FormFactor::kDesktop, now);
  devices.emplace_back("Device 1", "guid1",
                       syncer::DeviceInfo::FormFactor::kDesktop, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  base::HistogramTester histogram_tester;

  SendTabToSelfContextMenuDelegate delegate(web_contents(),
                                            ShareEntryPoint::kContentMenu);
  ui::SimpleMenuModel menu_model(&delegate);
  delegate.PopulateSubmenu(&menu_model);

  delegate.OnMenuWillShow(&menu_model);

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.TargetDeviceCount",
      static_cast<int>(SendTabToSelfDeviceCount::kTwoDevices), 1);
}

TEST_F(SendTabToSelfContextMenuDelegateTest,
       ExecuteCommandSendsMultipleTabsToDevice) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0",
                       syncer::DeviceInfo::FormFactor::kDesktop, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  // Set up first tab (default web_contents()).
  const GURL kUrl1("https://example1.com");
  const std::u16string kTitle1 = u"Title 1";
  NavigateAndCommit(kUrl1);
  content::NavigationEntry* entry1 =
      web_contents()->GetController().GetLastCommittedEntry();
  web_contents()->UpdateTitleForEntry(entry1, kTitle1);

  // Set up second tab.
  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  const GURL kUrl2("https://example2.com");
  const std::u16string kTitle2 = u"Title 2";
  content::WebContentsTester::For(web_contents2.get())
      ->NavigateAndCommit(kUrl2);
  content::NavigationEntry* entry2 =
      web_contents2->GetController().GetLastCommittedEntry();
  web_contents2->UpdateTitleForEntry(entry2, kTitle2);

  std::vector<content::WebContents*> web_contents_list = {web_contents(),
                                                          web_contents2.get()};

  SendTabToSelfContextMenuDelegate delegate(web_contents(), web_contents_list,
                                            ShareEntryPoint::kContentMenu);
  ui::SimpleMenuModel menu_model(&delegate);
  delegate.PopulateSubmenu(&menu_model);

  delegate.ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  std::vector<std::tuple<std::string, GURL, std::string>> sent_entries;
  for (const std::string& guid : model()->GetAllGuids()) {
    const SendTabToSelfEntry* entry = model()->GetEntryByGUID(guid);
    sent_entries.emplace_back(entry->GetTargetDeviceSyncCacheGuid(),
                              entry->GetURL(), entry->GetTitle());
  }
  EXPECT_THAT(sent_entries,
              UnorderedElementsAre(
                  std::make_tuple("guid0", kUrl1, base::UTF16ToUTF8(kTitle1)),
                  std::make_tuple("guid0", kUrl2, base::UTF16ToUTF8(kTitle2))));
}

TEST_F(SendTabToSelfContextMenuDelegateTest,
       ExecuteCommandSkipsDestroyedWebContents) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0",
                       syncer::DeviceInfo::FormFactor::kDesktop, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  const GURL kUrl1("https://example1.com");
  NavigateAndCommit(kUrl1);

  // Create a second tab and then immediately destroy it.
  auto web_contents2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  const GURL kUrl2("https://example2.com");
  content::WebContentsTester::For(web_contents2.get())
      ->NavigateAndCommit(kUrl2);

  std::vector<content::WebContents*> web_contents_list = {web_contents(),
                                                          web_contents2.get()};

  SendTabToSelfContextMenuDelegate delegate(web_contents(), web_contents_list,
                                            ShareEntryPoint::kContentMenu);

  // Destroy the second tab.
  web_contents2.reset();

  // Executing command should not crash and should send only the valid first
  // tab.
  delegate.ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  std::vector<std::string> guids = model()->GetAllGuids();
  ASSERT_EQ(guids.size(), 1u);
  const SendTabToSelfEntry* sent_entry = model()->GetEntryByGUID(guids[0]);
  EXPECT_EQ(sent_entry->GetURL(), kUrl1);
}

TEST_F(SendTabToSelfContextMenuDelegateTest, IsCommandIdEnabled) {
  SendTabToSelfContextMenuDelegate delegate(web_contents(),
                                            ShareEntryPoint::kContentMenu);

  // Command IDs handled by the Send Tab to Self submenu delegate.
  EXPECT_TRUE(delegate.IsCommandIdEnabled(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1));
  EXPECT_TRUE(delegate.IsCommandIdEnabled(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST));
  EXPECT_TRUE(delegate.IsCommandIdEnabled(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES));

  // Examples of command IDs not handled by this delegate.
  EXPECT_FALSE(delegate.IsCommandIdEnabled(IDC_COPY));
  EXPECT_FALSE(
      delegate.IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SHARING_SUBMENU));
}
}  // namespace

}  // namespace send_tab_to_self
