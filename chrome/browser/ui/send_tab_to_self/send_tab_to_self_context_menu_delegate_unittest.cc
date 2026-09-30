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
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

using FormFactor = syncer::DeviceInfo::FormFactor;
using OsType = syncer::DeviceInfo::OsType;

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

  StubSendTabToSelfSyncService* sync_service() {
    return static_cast<StubSendTabToSelfSyncService*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile()));
  }

  FakeSendTabToSelfModel* model() {
    return sync_service()->GetFakeSendTabToSelfModel();
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
                         "guid" + base::NumberToString(i), FormFactor::kDesktop,
                         OsType::kLinux, now);
  }
  model()->SetTargetDeviceInfoSortedList(devices);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu_model(delegate.get());
  delegate->PopulateSubmenu(&menu_model);

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
  devices.emplace_back("Device 0", "guid0", FormFactor::kDesktop,
                       OsType::kLinux, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  const GURL kExampleUrl("https://example.com");
  const std::u16string kExampleTitle = u"Example Title";
  NavigateAndCommit(kExampleUrl);
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  web_contents()->UpdateTitleForEntry(entry, kExampleTitle);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu_model(delegate.get());
  delegate->PopulateSubmenu(&menu_model);

  base::test::TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  delegate->ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  const SendTabToSelfEntry* sent_entry = future.Get();
  ASSERT_TRUE(sent_entry);
  ASSERT_EQ(model()->GetAllGuids().size(), 1u);
  EXPECT_EQ(sent_entry->GetTargetDeviceSyncCacheGuid(), "guid0");
  EXPECT_EQ(sent_entry->GetURL(), kExampleUrl);
  EXPECT_EQ(sent_entry->GetTitle(), base::UTF16ToUTF8(kExampleTitle));
}

// Tests that ExecuteCommand uses the target URL and target title passed to
// `MaybeCreateForTab` when sending to a device (e.g., when right-clicking a
// hyperlink).
TEST_F(SendTabToSelfContextMenuDelegateTest,
       ExecuteCommandSendsTargetUrlAndTitleWhenProvided) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0", FormFactor::kDesktop,
                       OsType::kLinux, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  const GURL kPageUrl("https://example.com/page");
  const GURL kLinkUrl("https://example.com/link");
  const std::string kLinkTitle = "Link Anchor Text";
  NavigateAndCommit(kPageUrl);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kLinkMenu, kLinkUrl, kLinkTitle);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu_model(delegate.get());
  delegate->PopulateSubmenu(&menu_model);

  delegate->ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

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
  devices.emplace_back("Device 0", "guid0", FormFactor::kDesktop,
                       OsType::kLinux, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  const GURL kPageUrl("https://example.com/page");
  const std::u16string kPageTitle = u"Page Title";
  const GURL kLinkUrl("https://example.com/link");
  NavigateAndCommit(kPageUrl);
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  web_contents()->UpdateTitleForEntry(entry, kPageTitle);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kLinkMenu, kLinkUrl);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu_model(delegate.get());
  delegate->PopulateSubmenu(&menu_model);

  delegate->ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  std::vector<std::string> guids = model()->GetAllGuids();
  ASSERT_EQ(guids.size(), 1u);
  const SendTabToSelfEntry* sent_entry = model()->GetEntryByGUID(guids[0]);
  EXPECT_EQ(sent_entry->GetURL(), kLinkUrl);
  EXPECT_EQ(sent_entry->GetTitle(), base::UTF16ToUTF8(kPageTitle));
}

// Tests that PopulateSubmenu correctly adds the device items and the "Manage
// Devices" item to the menu model with the expected localized label.
TEST_F(SendTabToSelfContextMenuDelegateTest,
       PopulateSubmenuAddsDevicesAndManageItem) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0", FormFactor::kDesktop,
                       OsType::kLinux, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu_model(delegate.get());
  delegate->PopulateSubmenu(&menu_model);

  // Expect: 1 device item + 1 separator + 1 manage devices item = 3 items.
  ASSERT_EQ(menu_model.GetItemCount(), 3u);
  EXPECT_EQ(menu_model.GetCommandIdAt(0),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1);
  EXPECT_EQ(menu_model.GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(menu_model.GetCommandIdAt(2),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES);
  EXPECT_EQ(menu_model.GetLabelAt(2),
            l10n_util::GetStringUTF16(
                IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_MANAGE_DEVICES));
}

// Tests that PopulateSubmenu uses sentence case for the three-dot share menu
// across all platforms including macOS.
TEST_F(SendTabToSelfContextMenuDelegateTest,
       PopulateSubmenuUsesSentenceCaseForShareMenu) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0", FormFactor::kDesktop,
                       OsType::kLinux, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kShareMenu);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu_model(delegate.get());
  delegate->PopulateSubmenu(&menu_model);

  ASSERT_EQ(menu_model.GetItemCount(), 3u);
  EXPECT_EQ(menu_model.GetLabelAt(2),
            l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_MANAGE_DEVICES));
}

// Tests that OnMenuWillShow correctly records device count metrics.
TEST_F(SendTabToSelfContextMenuDelegateTest, OnMenuWillShowRecordsMetrics) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0", FormFactor::kDesktop,
                       OsType::kLinux, now);
  devices.emplace_back("Device 1", "guid1", FormFactor::kDesktop,
                       OsType::kLinux, now);
  model()->SetTargetDeviceInfoSortedList(devices);

  base::HistogramTester histogram_tester;

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu_model(delegate.get());
  delegate->PopulateSubmenu(&menu_model);

  delegate->OnMenuWillShow(&menu_model);

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.TargetDeviceCount",
      static_cast<int>(SendTabToSelfDeviceCount::kTwoDevices), 1);
}

TEST_F(SendTabToSelfContextMenuDelegateTest,
       ExecuteCommandSendsMultipleTabsToDevice) {
  base::Time now = base::Time::Now();
  std::vector<TargetDeviceInfo> devices;
  devices.emplace_back("Device 0", "guid0", FormFactor::kDesktop,
                       OsType::kLinux, now);
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

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForMultipleTabs(
          web_contents(), web_contents_list, ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu_model(delegate.get());
  delegate->PopulateSubmenu(&menu_model);

  base::test::TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  delegate->ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  std::vector<std::tuple<std::string, GURL, std::string>> sent_entries;
  for (size_t i = 0; i < web_contents_list.size(); ++i) {
    const SendTabToSelfEntry* entry = future.Take();
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
  devices.emplace_back("Device 0", "guid0", FormFactor::kDesktop,
                       OsType::kLinux, now);
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

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForMultipleTabs(
          web_contents(), web_contents_list, ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);

  // Destroy the second tab.
  web_contents2.reset();

  base::test::TestFuture<const SendTabToSelfEntry*> future;
  model()->SetSendEntryCallback(future.GetRepeatingCallback());

  // Executing command should not crash and should send only the valid first
  // tab.
  delegate->ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  const SendTabToSelfEntry* sent_entry = future.Get();
  ASSERT_TRUE(sent_entry);
  ASSERT_EQ(model()->GetAllGuids().size(), 1u);
  EXPECT_EQ(sent_entry->GetURL(), kUrl1);
}

// Tests that `IsCommandIdEnabled` returns true only for Send Tab to Self
// submenu commands.
TEST_F(SendTabToSelfContextMenuDelegateTest, IsCommandIdEnabled) {
  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);

  // Command IDs handled by the Send Tab to Self submenu delegate.
  EXPECT_TRUE(delegate->IsCommandIdEnabled(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1));
  EXPECT_TRUE(delegate->IsCommandIdEnabled(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE_LAST));
  EXPECT_TRUE(delegate->IsCommandIdEnabled(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES));
  EXPECT_TRUE(delegate->IsCommandIdEnabled(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN));
  EXPECT_TRUE(delegate->IsCommandIdEnabled(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN_ON_PHONE));

  // Examples of command IDs not handled by this delegate.
  EXPECT_FALSE(delegate->IsCommandIdEnabled(IDC_COPY));
  EXPECT_FALSE(
      delegate->IsCommandIdEnabled(IDC_CONTENT_CONTEXT_SHARING_SUBMENU));
}

// Tests that `MaybeCreateForTab` and `MaybeCreateForMultipleTabs` return
// nullptr when any precondition for showing the submenu is not met.
TEST_F(SendTabToSelfContextMenuDelegateTest,
       MaybeCreateReturnsNullWhenPreconditionsNotMet) {
  EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                /*web_contents=*/nullptr, ShareEntryPoint::kContentMenu),
            nullptr);

  std::vector<content::WebContents*> web_contents_list = {web_contents()};
  EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForMultipleTabs(
                /*primary_web_contents=*/nullptr, web_contents_list,
                ShareEntryPoint::kTabMenu),
            nullptr);

  // Returns nullptr when `GetEntryPointDisplayReason` is `std::nullopt`.
  sync_service()->SetEntryPointDisplayReason(std::nullopt);
  EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                web_contents(), ShareEntryPoint::kContentMenu),
            nullptr);

  // Returns nullptr when `ShouldShowSubmenu` is false for the display reason.
  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);
  EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                web_contents(), ShareEntryPoint::kContentMenu),
            nullptr);

  // Returns nullptr when `kOfferFeature` is returned but no target devices are
  // available.
  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  model()->SetTargetDeviceInfoSortedList({});
  EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                web_contents(), ShareEntryPoint::kContentMenu),
            nullptr);
}

// Tests that `MaybeCreateForTab` allows `kOfferFeature` unconditionally, gates
// `kOfferSignIn` and `kOfferReauth` behind `kSendTabToSelfSubmenuSigninPromos`,
// and requires both `kSendTabToSelfSubmenuSigninPromos` and
// `kSendTabToSelfNoTargetDeviceQrCode` for `kInformNoTargetDevice`.
TEST_F(SendTabToSelfContextMenuDelegateTest, MaybeCreateForTabByDisplayReason) {
  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferFeature);
  EXPECT_NE(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                web_contents(), ShareEntryPoint::kContentMenu),
            nullptr);

  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);
  EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                web_contents(), ShareEntryPoint::kContentMenu),
            nullptr);

  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kInformNoTargetDevice);
  EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                web_contents(), ShareEntryPoint::kContentMenu),
            nullptr);

  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferReauth);
  EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                web_contents(), ShareEntryPoint::kContentMenu),
            nullptr);

  {
    base::test::ScopedFeatureList feature_list(
        kSendTabToSelfSubmenuSigninPromos);
    sync_service()->SetEntryPointDisplayReason(
        EntryPointDisplayReason::kOfferSignIn);
    EXPECT_NE(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                  web_contents(), ShareEntryPoint::kContentMenu),
              nullptr);

    sync_service()->SetEntryPointDisplayReason(
        EntryPointDisplayReason::kOfferReauth);
    EXPECT_NE(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                  web_contents(), ShareEntryPoint::kContentMenu),
              nullptr);

    sync_service()->SetEntryPointDisplayReason(
        EntryPointDisplayReason::kInformNoTargetDevice);
    EXPECT_EQ(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                  web_contents(), ShareEntryPoint::kContentMenu),
              nullptr);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {kSendTabToSelfSubmenuSigninPromos, kSendTabToSelfNoTargetDeviceQrCode},
        {});
    sync_service()->SetEntryPointDisplayReason(
        EntryPointDisplayReason::kInformNoTargetDevice);
    EXPECT_NE(SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
                  web_contents(), ShareEntryPoint::kContentMenu),
              nullptr);
  }
#endif
}

class SendTabToSelfContextMenuDelegateSigninPromosTest
    : public SendTabToSelfContextMenuDelegateTest {
 public:
  SendTabToSelfContextMenuDelegateSigninPromosTest() {
    feature_list_.InitWithFeatures({kSendTabToSelfSubmenuSigninPromos,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
                                    kSendTabToSelfNoTargetDeviceQrCode
#endif
                                   },
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that `PopulateSubmenu` adds the "Not signed in" title and "Sign in to
// Chrome" action item with an icon for `kOfferSignIn`, using context-menu
// capitalization for context menus and sentence case for the three-dot share
// menu, and that `OnMenuWillShow` does not record target device count metrics.
TEST_F(SendTabToSelfContextMenuDelegateSigninPromosTest,
       PopulateSubmenuForOfferSignIn) {
  base::HistogramTester histogram_tester;
  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> content_delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(content_delegate);
  ui::SimpleMenuModel content_menu(content_delegate.get());
  content_delegate->PopulateSubmenu(&content_menu);
  ASSERT_EQ(content_menu.GetItemCount(), 2u);
  EXPECT_EQ(content_menu.GetTypeAt(0), ui::MenuModel::TYPE_TITLE);
  EXPECT_EQ(content_menu.GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE));
  EXPECT_EQ(content_menu.GetCommandIdAt(1),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN);
  EXPECT_EQ(
      content_menu.GetLabelAt(1),
      l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SIGN_IN));
  EXPECT_FALSE(content_menu.GetIconAt(1).IsEmpty());
  content_delegate->OnMenuWillShow(&content_menu);
  histogram_tester.ExpectTotalCount("Sharing.SendTabToSelf.TargetDeviceCount",
                                    0);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> share_delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kShareMenu);
  ASSERT_TRUE(share_delegate);
  ui::SimpleMenuModel share_menu(share_delegate.get());
  share_delegate->PopulateSubmenu(&share_menu);
  ASSERT_EQ(share_menu.GetItemCount(), 2u);
  EXPECT_EQ(share_menu.GetLabelAt(1),
            l10n_util::GetStringUTF16(
                IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_BUTTON_LABEL));
}

// Tests that `PopulateSubmenu` adds the "Not signed in" title and "Sign in to
// Chrome" action item with an icon for `kOfferReauth`.
TEST_F(SendTabToSelfContextMenuDelegateSigninPromosTest,
       PopulateSubmenuForOfferReauth) {
  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferReauth);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);
  ui::SimpleMenuModel menu(delegate.get());
  delegate->PopulateSubmenu(&menu);
  ASSERT_EQ(menu.GetItemCount(), 2u);
  EXPECT_EQ(menu.GetTypeAt(0), ui::MenuModel::TYPE_TITLE);
  EXPECT_EQ(menu.GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE));
  EXPECT_EQ(menu.GetCommandIdAt(1),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN);
  EXPECT_EQ(menu.GetLabelAt(1), l10n_util::GetStringUTF16(
                                    IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SIGN_IN));
  EXPECT_FALSE(menu.GetIconAt(1).IsEmpty());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests that `PopulateSubmenu` adds the "No other device found" title and "Sign
// in on your phone" action item with an icon for `kInformNoTargetDevice`, using
// context-menu capitalization for context menus and sentence case for the
// three-dot share menu.
TEST_F(SendTabToSelfContextMenuDelegateSigninPromosTest,
       PopulateSubmenuForInformNoTargetDevice) {
  sync_service()->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kInformNoTargetDevice);

  std::unique_ptr<SendTabToSelfContextMenuDelegate> content_delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(content_delegate);
  ui::SimpleMenuModel content_menu(content_delegate.get());
  content_delegate->PopulateSubmenu(&content_menu);
  ASSERT_EQ(content_menu.GetItemCount(), 2u);
  EXPECT_EQ(content_menu.GetTypeAt(0), ui::MenuModel::TYPE_TITLE);
  EXPECT_EQ(content_menu.GetLabelAt(0),
            l10n_util::GetStringUTF16(
                IDS_SEND_TAB_TO_SELF_NO_OTHER_DEVICE_FOUND_TITLE));
  EXPECT_EQ(content_menu.GetCommandIdAt(1),
            IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN_ON_PHONE);
  EXPECT_EQ(
      content_menu.GetLabelAt(1),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_ON_PHONE_BUTTON_LABEL));
  EXPECT_FALSE(content_menu.GetIconAt(1).IsEmpty());

  std::unique_ptr<SendTabToSelfContextMenuDelegate> share_delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents(), ShareEntryPoint::kShareMenu);
  ASSERT_TRUE(share_delegate);
  ui::SimpleMenuModel share_menu(share_delegate.get());
  share_delegate->PopulateSubmenu(&share_menu);
  ASSERT_EQ(share_menu.GetItemCount(), 2u);
  EXPECT_EQ(share_menu.GetLabelAt(1),
            l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_SIGN_IN_ON_PHONE));
}
#endif

// Tests that ExecuteCommand does not crash when called for the "Manage Devices"
// or sign-in promo commands with a null or destroyed web contents.
TEST_F(SendTabToSelfContextMenuDelegateTest,
       ExecuteCommandManageDevicesWithDestroyedWebContentsDoesNotCrash) {
  auto web_contents2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<SendTabToSelfContextMenuDelegate> delegate =
      SendTabToSelfContextMenuDelegate::MaybeCreateForTab(
          web_contents2.get(), ShareEntryPoint::kContentMenu);
  ASSERT_TRUE(delegate);
  // Destroy web contents before executing command.
  web_contents2.reset();

  delegate->ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_MANAGE_DEVICES,
                           0);
  delegate->ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN, 0);
  delegate->ExecuteCommand(
      IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_SIGN_IN_ON_PHONE, 0);
}

}  // namespace

}  // namespace send_tab_to_self
