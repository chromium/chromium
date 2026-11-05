// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/other_device_tabs_sub_menu_model.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder_test_helper.h"
#include "chrome/browser/ui/tabs/remote_tabs_menu_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/menus/simple_menu_model.h"

using ::testing::ElementsAre;

namespace {

class MockMenuModelDelegate : public ui::MenuModelDelegate {
 public:
  MOCK_METHOD(void, OnIconChanged, (int command_id), (override));
};

struct ModelData {
  ui::MenuModel::ItemType type;
  bool enabled;
};

void VerifyModel(const ui::MenuModel& model, base::span<const ModelData> data) {
  ASSERT_EQ(data.size(), model.GetItemCount());
  for (size_t i = 0; i < data.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(data[i].type, model.GetTypeAt(i));
    EXPECT_EQ(data[i].enabled, model.IsEnabledAt(i));
  }
}

// Title given to tab `tab_index` of device `device_index` by
// `RegisterDevices()`.
std::u16string TabTitle(size_t device_index, size_t tab_index) {
  return u"D" + base::NumberToString16(device_index) + u" Tab " +
         base::NumberToString16(tab_index + 1);
}

// Returns the labels of the items in `[begin, end)` of `model`.
std::vector<std::u16string> LabelsInRange(const ui::MenuModel& model,
                                          size_t begin,
                                          size_t end) {
  std::vector<std::u16string> labels;
  for (size_t i = begin; i < end; ++i) {
    labels.push_back(model.GetLabelAt(i));
  }
  return labels;
}

std::unique_ptr<AppMenuModel> CreateAppMenuModel(
    BrowserWindowInterface* target_browser) {
  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, target_browser);
  app_menu_model->Init();
  return app_menu_model;
}

class OtherDeviceTabsSubMenuModelTest : public InProcessBrowserTest {
 public:
  OtherDeviceTabsSubMenuModelTest()
      : OtherDeviceTabsSubMenuModelTest(/*enable_feature=*/true) {}

  // Starts session sync so that OpenTabsUIDelegate becomes available.
  void StartSessionSync() {
    auto* session_sync_service =
        SessionSyncServiceFactory::GetForProfile(browser()->GetProfile());

    syncer::DataTypeActivationRequest activation_request;
    activation_request.cache_guid = "test_cache_guid";
    activation_request.error_handler = base::DoNothing();

    std::unique_ptr<syncer::DataTypeActivationResponse> activation_response;
    base::RunLoop loop;
    session_sync_service->GetControllerDelegate()->OnSyncStarting(
        activation_request,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<syncer::DataTypeActivationResponse> response) {
              activation_response = std::move(response);
              loop.Quit();
            }));
    loop.Run();
    ASSERT_NE(nullptr, activation_response);
    ASSERT_NE(nullptr, activation_response->type_processor);
    sync_processor_ = std::move(activation_response->type_processor);
    // ClientTagBasedDataTypeProcessor requires connecting before other
    // interactions with the worker happen.
    sync_processor_->ConnectSync(
        std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());
  }

  void SignIn() {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
        "test@gmail.com", signin::ConsentLevel::kSignin);
  }

  void SetTabSyncSelected(bool selected) {
    SyncServiceFactory::GetForProfile(browser()->GetProfile())
        ->GetUserSettings()
        ->SetSelectedType(syncer::UserSelectableType::kTabs, selected);
  }

  // Signs in, turns tab sync on and starts session sync: the preconditions
  // for the submenu to list devices.
  void SetUpSignedInWithTabSync() {
    SignIn();
    SetTabSyncSelected(true);
    StartSessionSync();
  }

  // Registers one more session (device) per entry of `num_tabs_per_device`,
  // each with one window holding that many tabs. Tab `t` of device `s` is
  // titled `TabTitle(s, t)`. Timestamps are explicit so that, within a call,
  // *later* devices and lower tab indices are strictly more recent; the last
  // registered device is therefore expected first in the menu.
  void RegisterDevices(const std::vector<int>& num_tabs_per_device) {
    const base::Time now = base::Time::Now();
    const size_t num_devices = num_tabs_per_device.size();
    const size_t first_device = recent_tabs_builder_.GetSessionCount();
    bool all_devices_have_tabs = true;
    for (size_t i = 0; i < num_devices; ++i) {
      const size_t s = first_device + i;
      recent_tabs_builder_.AddSession();
      recent_tabs_builder_.AddWindow(s);
      all_devices_have_tabs &= num_tabs_per_device[i] > 0;
      for (int t = 0; t < num_tabs_per_device[i]; ++t) {
        recent_tabs_builder_.AddTabWithInfo(
            s, 0, now - base::Minutes(100 * (num_devices - 1 - i) + t),
            TabTitle(s, t));
      }
    }
    recent_tabs_builder_.ExportToSessionSync(sync_processor_.get());
    // A session without tabs is not "presentable" and never comes back from
    // GetAllForeignSessions(), which VerifyExport() would flag.
    if (all_devices_have_tabs) {
      recent_tabs_builder_.VerifyExport(
          SessionSyncServiceFactory::GetForProfile(browser()->GetProfile())
              ->GetOpenTabsUIDelegate());
    }
  }

  // The device label shown for the device registered at `device_index`.
  std::u16string DeviceName(size_t device_index) {
    // Mirrors RecentTabsBuilderTestHelper's session naming.
    return u"session_name" +
           base::NumberToString16(
               recent_tabs_builder_.GetSessionID(device_index).id());
  }

  std::unique_ptr<OtherDeviceTabsSubMenuModel> CreateModel() {
    return std::make_unique<OtherDeviceTabsSubMenuModel>(browser(),
                                                         base::DoNothing());
  }

 protected:
  explicit OtherDeviceTabsSubMenuModelTest(bool enable_feature) {
    feature_list_.InitWithFeatureState(syncer::kCrossDeviceTabsInAppMenu,
                                       enable_feature);
  }

  std::unique_ptr<syncer::DataTypeProcessor> sync_processor_;
  RecentTabsBuilderTestHelper recent_tabs_builder_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// --- Flag off --------------------------------------------------------------

class OtherDeviceTabsSubMenuModelFlagOffTest
    : public OtherDeviceTabsSubMenuModelTest {
 public:
  OtherDeviceTabsSubMenuModelFlagOffTest()
      : OtherDeviceTabsSubMenuModelTest(/*enable_feature=*/false) {}
};
IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelFlagOffTest, NoAppMenuRow) {
  std::unique_ptr<AppMenuModel> app_menu_model = CreateAppMenuModel(browser());

  EXPECT_FALSE(
      app_menu_model
          ->GetIndexOfCommandId(AppMenuModel::kOtherDeviceTabsMenuPlaceholder)
          .has_value());
  EXPECT_FALSE(app_menu_model->IsCommandIdEnabled(IDC_OTHER_DEVICE_TABS_MENU));
}

// --- Non-eligible state -----------------------------------------------------

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest, EmptyWhenNotEligible) {
  SetUpSignedInWithTabSync();
  RegisterDevices({3});
  ASSERT_EQ(4u, CreateModel()->GetItemCount());

  PrefService* pref_service = browser()->GetProfile()->GetPrefs();
  pref_service->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);
  EXPECT_EQ(0u, CreateModel()->GetItemCount());
  pref_service->SetBoolean(prefs::kSavingBrowserHistoryDisabled, false);
  ASSERT_EQ(4u, CreateModel()->GetItemCount());

#if !BUILDFLAG(IS_CHROMEOS)
  // ShouldShowTabsFromOtherDevices() doesn't check sync settings on ChromeOS.
  SetTabSyncSelected(false);
  EXPECT_EQ(0u, CreateModel()->GetItemCount());
  SetTabSyncSelected(true);
  ASSERT_EQ(4u, CreateModel()->GetItemCount());

  // Equivalent of the SyncDisabled enterprise policy.
  pref_service->SetBoolean(syncer::prefs::internal::kSyncManaged, true);
  EXPECT_EQ(0u, CreateModel()->GetItemCount());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

// --- Active: inline device headers and tabs --------------------------------

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest,
                       DeviceWithoutTabsIsSkipped) {
  SetUpSignedInWithTabSync();
  RegisterDevices({0, 2});

  std::unique_ptr<OtherDeviceTabsSubMenuModel> model = CreateModel();
  VerifyModel(*model, {{{ui::MenuModel::TYPE_TITLE, false},
                        {ui::MenuModel::TYPE_COMMAND, true},
                        {ui::MenuModel::TYPE_COMMAND, true}}});
  EXPECT_THAT(LabelsInRange(*model, 0, model->GetItemCount()),
              ElementsAre(DeviceName(1), TabTitle(1, 0), TabTitle(1, 1)));
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest,
                       ShowsAllDevicesWithoutTruncation) {
  SetUpSignedInWithTabSync();
  // Register 10 devices, each with 1 tab.
  constexpr size_t kNumDevices = 10;
  RegisterDevices(std::vector<int>(kNumDevices, 1));

  std::unique_ptr<OtherDeviceTabsSubMenuModel> model = CreateModel();
  // Each device contributes 1 title + 1 tab, separated by kNumDevices - 1
  // dividers.
  ASSERT_EQ(3 * kNumDevices - 1, model->GetItemCount());

  // Devices should be listed most recent first (reverse registration order).
  for (size_t i = 0; i < kNumDevices; ++i) {
    EXPECT_EQ(ui::MenuModel::TYPE_TITLE, model->GetTypeAt(3 * i));
    EXPECT_EQ(DeviceName(kNumDevices - 1 - i), model->GetLabelAt(3 * i));
  }
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest,
                       DevicesAndTabsOrderedByRecency) {
  SetUpSignedInWithTabSync();
  RegisterDevices({2, 2, 2});

  std::unique_ptr<OtherDeviceTabsSubMenuModel> model = CreateModel();
  ASSERT_NO_FATAL_FAILURE(
      VerifyModel(*model, {{{ui::MenuModel::TYPE_TITLE, false},
                            {ui::MenuModel::TYPE_COMMAND, true},
                            {ui::MenuModel::TYPE_COMMAND, true},
                            {ui::MenuModel::TYPE_SEPARATOR, true},
                            {ui::MenuModel::TYPE_TITLE, false},
                            {ui::MenuModel::TYPE_COMMAND, true},
                            {ui::MenuModel::TYPE_COMMAND, true},
                            {ui::MenuModel::TYPE_SEPARATOR, true},
                            {ui::MenuModel::TYPE_TITLE, false},
                            {ui::MenuModel::TYPE_COMMAND, true},
                            {ui::MenuModel::TYPE_COMMAND, true}}}));

  // Devices are listed most recent first (reverse registration order), with
  // each device's tabs directly below its title in recency order.
  for (size_t i = 0; i < 3; ++i) {
    const size_t base = 4 * i;
    const size_t device_index = 2 - i;
    EXPECT_THAT(LabelsInRange(*model, base, base + 3),
                ElementsAre(DeviceName(device_index), TabTitle(device_index, 0),
                            TabTitle(device_index, 1)));
    EXPECT_FALSE(model->GetIconAt(base + 1).IsEmpty());
    EXPECT_FALSE(model->GetIconAt(base + 2).IsEmpty());
  }
}

// --- Active: interaction ---------------------------------------------------

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest, OpenTab) {
  SetUpSignedInWithTabSync();
  RegisterDevices({1});

  std::unique_ptr<OtherDeviceTabsSubMenuModel> model = CreateModel();
  ASSERT_EQ(2u, model->GetItemCount());
  const std::vector<RecentTabItem> items =
      RecentTabsBuilder::BuildOtherDeviceTabs(browser()->GetProfile());
  ASSERT_EQ(2u, items.size());
  const GURL expected_url = items[1].url();

  TabStripModel* const tab_strip = browser()->tab_strip_model();
  const int tab_count = tab_strip->count();
  content::WebContents* const initial_active_contents =
      tab_strip->GetActiveWebContents();
  base::UserActionTester user_action_tester;

  // Activating a device title header should not open a tab.
  model->ActivatedAt(0);
  EXPECT_EQ(tab_count, tab_strip->count());
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "WrenchMenu_OpenRecentTabFromDevice"));

  // Middle-click opens the tab in the background without activating it.
  model->ActivatedAt(1, ui::EF_MIDDLE_MOUSE_BUTTON);
  EXPECT_EQ(tab_count + 1, tab_strip->count());
  EXPECT_EQ(initial_active_contents, tab_strip->GetActiveWebContents());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "WrenchMenu_OpenRecentTabFromDevice"));

  // Activating with default flags opens the tab in the foreground.
  model->ActivatedAt(1);
  EXPECT_EQ(tab_count + 2, tab_strip->count());
  EXPECT_NE(initial_active_contents, tab_strip->GetActiveWebContents());
  EXPECT_EQ(expected_url, tab_strip->GetActiveWebContents()->GetVisibleURL());
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "WrenchMenu_OpenRecentTabFromDevice"));
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest, MoreTabsFlyout) {
  SetUpSignedInWithTabSync();
  constexpr size_t kNumTabs = 12;

  // Expects each device to show `num_inline` tabs and a flyout with the rest.
  auto expect_inline_tabs = [&](size_t num_devices, size_t num_inline) {
    std::unique_ptr<OtherDeviceTabsSubMenuModel> model = CreateModel();
    // Per device: title, inline tabs, flyout and a divider (except the last).
    ASSERT_EQ(num_devices * (num_inline + 3) - 1, model->GetItemCount());
    for (size_t i = num_inline + 1; i < model->GetItemCount();
         i += num_inline + 3) {
      EXPECT_EQ(ui::MenuModel::TYPE_SUBMENU, model->GetTypeAt(i));
      EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                    IDS_OTHER_DEVICE_TABS_SEE_MORE_TABS,
                    static_cast<int>(kNumTabs - num_inline)),
                model->GetLabelAt(i));
      EXPECT_EQ(remote_tabs_menu_util::GetDeviceFormFactorIcon(
                    syncer::DeviceInfo::FormFactor::kDesktop),
                model->GetIconAt(i));
      EXPECT_EQ(kNumTabs - num_inline,
                model->GetSubmenuModelAt(i)->GetItemCount());
    }
  };

  RegisterDevices({kNumTabs});
  expect_inline_tabs(1, 10);

  // The flyout lists the remaining tabs, which open like inline ones.
  base::MockCallback<OtherDeviceTabsSubMenuModel::LogMenuMetricsCallback>
      log_menu_metrics;
  auto model = std::make_unique<OtherDeviceTabsSubMenuModel>(
      browser(), log_menu_metrics.Get());
  ui::MenuModel* flyout = model->GetSubmenuModelAt(11);
  ASSERT_NE(nullptr, flyout);
  EXPECT_THAT(LabelsInRange(*flyout, 0, flyout->GetItemCount()),
              ElementsAre(TabTitle(0, 10), TabTitle(0, 11)));
  EXPECT_FALSE(flyout->GetIconAt(1).IsEmpty());
  const GURL expected_url =
      RecentTabsBuilder::BuildOtherDeviceTabs(browser()->GetProfile())
          .back()
          .children()[1]
          .url();
  TabStripModel* const tab_strip = browser()->tab_strip_model();
  const int tab_count = tab_strip->count();
  content::WebContents* const initial_active_contents =
      tab_strip->GetActiveWebContents();
  EXPECT_CALL(log_menu_metrics, Run(IDC_OPEN_OTHER_DEVICE_TAB)).Times(2);
  flyout->ActivatedAt(1, ui::EF_MIDDLE_MOUSE_BUTTON);
  EXPECT_EQ(tab_count + 1, tab_strip->count());
  EXPECT_EQ(initial_active_contents, tab_strip->GetActiveWebContents());
  flyout->ActivatedAt(1);
  EXPECT_EQ(tab_count + 2, tab_strip->count());
  EXPECT_EQ(expected_url, tab_strip->GetActiveWebContents()->GetVisibleURL());

  RegisterDevices({kNumTabs});
  expect_inline_tabs(2, 5);

  RegisterDevices({kNumTabs});
  expect_inline_tabs(3, 5);

  RegisterDevices({kNumTabs});
  expect_inline_tabs(4, 3);
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest,
                       MoreTabsFlyoutOnlyAboveCap) {
  SetUpSignedInWithTabSync();
  RegisterDevices({10});
  // Title and all 10 tabs inline.
  EXPECT_EQ(11u, CreateModel()->GetItemCount());

  recent_tabs_builder_.SetSessionFormFactor(
      0, syncer::DeviceInfo::FormFactor::kPhone);
  RegisterDevices({6});
  std::unique_ptr<OtherDeviceTabsSubMenuModel> model = CreateModel();
  // Per device: title, 5 tabs and a flyout, with a divider in between.
  ASSERT_EQ(15u, model->GetItemCount());
  EXPECT_EQ(u"See 1 more tab", model->GetLabelAt(6));
  EXPECT_EQ(remote_tabs_menu_util::GetDeviceFormFactorIcon(
                syncer::DeviceInfo::FormFactor::kDesktop),
            model->GetIconAt(6));
  ui::MenuModel* const singular_flyout = model->GetSubmenuModelAt(6);
  EXPECT_THAT(
      LabelsInRange(*singular_flyout, 0, singular_flyout->GetItemCount()),
      ElementsAre(TabTitle(1, 5)));
  EXPECT_EQ(u"See 5 more tabs", model->GetLabelAt(14));
  EXPECT_EQ(remote_tabs_menu_util::GetDeviceFormFactorIcon(
                syncer::DeviceInfo::FormFactor::kPhone),
            model->GetIconAt(14));
  EXPECT_EQ(5u, model->GetSubmenuModelAt(14)->GetItemCount());
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest, MoreTabsFlyoutFavicon) {
  SetUpSignedInWithTabSync();
  RegisterDevices({11});
  Profile* const profile = browser()->GetProfile();
  const GURL url = RecentTabsBuilder::BuildOtherDeviceTabs(profile)
                       .back()
                       .children()[0]
                       .url();
  HistoryServiceFactory::GetForProfile(profile,
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorRED);
  FaviconServiceFactory::GetForProfile(profile,
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetFavicons({url}, GURL("http://foo/favicon.ico"),
                    favicon_base::IconType::kFavicon,
                    gfx::Image::CreateFrom1xBitmap(bitmap));

  // The root model's delegate is told which flyout row to refresh.
  testing::StrictMock<MockMenuModelDelegate> delegate;
  std::unique_ptr<OtherDeviceTabsSubMenuModel> model = CreateModel();
  ui::MenuModel* const flyout = model->GetSubmenuModelAt(11);
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnIconChanged(flyout->GetCommandIdAt(0)))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  model->SetMenuModelDelegate(&delegate);
  run_loop.Run();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      bitmap, *flyout->GetIconAt(0).GetImage().ToSkBitmap()));
  model->SetMenuModelDelegate(nullptr);
}

// --- App menu integration --------------------------------------------------

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest, AppMenuOpenTab) {
  SetUpSignedInWithTabSync();
  RegisterDevices({1});

  const std::vector<RecentTabItem> items =
      RecentTabsBuilder::BuildOtherDeviceTabs(browser()->GetProfile());
  ASSERT_EQ(2u, items.size());
  const GURL expected_url = items[1].url();

  std::unique_ptr<AppMenuModel> app_menu_model = CreateAppMenuModel(browser());
  const std::optional<size_t> other_device_tabs_index =
      app_menu_model->GetIndexOfCommandId(
          AppMenuModel::kOtherDeviceTabsMenuPlaceholder);
  ASSERT_TRUE(other_device_tabs_index.has_value());
  ui::MenuModel* submenu =
      app_menu_model->GetSubmenuModelAt(other_device_tabs_index.value());
  ASSERT_NE(nullptr, submenu);
  ASSERT_EQ(2u, submenu->GetItemCount());

  const int tab_count = browser()->tab_strip_model()->count();
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  submenu->ActivatedAt(1);

  EXPECT_EQ(tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      expected_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  histogram_tester.ExpectBucketCount("WrenchMenu.MenuAction",
                                     MENU_ACTION_OTHER_DEVICE_TAB, 1);
  histogram_tester.ExpectTotalCount(
      "WrenchMenu.TimeToAction.OpenOtherDeviceTab", 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "WrenchMenu_OpenRecentTabFromDevice"));
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest, AppMenuPlacement) {
  SetUpSignedInWithTabSync();
  RegisterDevices({1});

  std::unique_ptr<AppMenuModel> app_menu_model = CreateAppMenuModel(browser());

  const std::optional<size_t> other_device_tabs_index =
      app_menu_model->GetIndexOfCommandId(
          AppMenuModel::kOtherDeviceTabsMenuPlaceholder);
  const std::optional<size_t> history_index =
      app_menu_model->GetIndexOfCommandId(
          AppMenuModel::kRecentTabsMenuPlaceholder);
  ASSERT_TRUE(other_device_tabs_index.has_value());
  ASSERT_TRUE(history_index.has_value());
  // "Other device tabs" sits directly below "History".
  EXPECT_EQ(history_index.value() + 1, other_device_tabs_index.value());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OTHER_DEVICE_TABS_MENU),
            app_menu_model->GetLabelAt(other_device_tabs_index.value()));
  EXPECT_FALSE(
      app_menu_model->GetIconAt(other_device_tabs_index.value()).IsEmpty());
  EXPECT_EQ(
      AppMenuModel::kOtherDeviceTabsMenuItem,
      app_menu_model->GetElementIdentifierAt(other_device_tabs_index.value()));
  // The row must be enabled or the views layer will not open the submenu.
  EXPECT_TRUE(app_menu_model->IsEnabledAt(other_device_tabs_index.value()));
  EXPECT_NE(nullptr,
            app_menu_model->GetSubmenuModelAt(other_device_tabs_index.value()));
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest,
                       NoAppMenuRowInIncognito) {
  SetUpSignedInWithTabSync();
  RegisterDevices({1});

  std::unique_ptr<AppMenuModel> app_menu_model =
      CreateAppMenuModel(CreateIncognitoBrowser());

  EXPECT_FALSE(
      app_menu_model
          ->GetIndexOfCommandId(AppMenuModel::kOtherDeviceTabsMenuPlaceholder)
          .has_value());
  EXPECT_TRUE(RecentTabsBuilder::BuildOtherDeviceTabs(
                  browser()->GetProfile()->GetPrimaryOTRProfile(
                      /*create_if_needed=*/true))
                  .empty());
}

}  // namespace
