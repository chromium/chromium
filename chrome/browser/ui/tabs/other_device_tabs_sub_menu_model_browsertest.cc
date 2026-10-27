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
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder_test_helper.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/history/core/common/pref_names.h"
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
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"

using ::testing::ElementsAre;

namespace {

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

  // Registers one session (device) per entry of `num_tabs_per_device`, each
  // with one window holding that many tabs. Tab `t` of device `s` is titled
  // `TabTitle(s, t)`. Timestamps are explicit so that *later* devices and
  // lower tab indices are strictly more recent; the last registered device is
  // therefore expected first in the menu.
  void RegisterDevices(const std::vector<int>& num_tabs_per_device) {
    const base::Time now = base::Time::Now();
    const size_t num_devices = num_tabs_per_device.size();
    bool all_devices_have_tabs = true;
    for (size_t s = 0; s < num_devices; ++s) {
      recent_tabs_builder_.AddSession();
      recent_tabs_builder_.AddWindow(s);
      all_devices_have_tabs &= num_tabs_per_device[s] > 0;
      for (int t = 0; t < num_tabs_per_device[s]; ++t) {
        recent_tabs_builder_.AddTabWithInfo(
            s, 0, now - base::Minutes(100 * (num_devices - 1 - s) + t),
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
    return std::make_unique<OtherDeviceTabsSubMenuModel>(browser());
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

// --- App menu integration --------------------------------------------------

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
