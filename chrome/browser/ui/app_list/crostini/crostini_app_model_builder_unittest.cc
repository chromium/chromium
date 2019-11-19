// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

using crostini::CrostiniTestHelper;
using ::testing::_;
using ::testing::Matcher;

namespace {

constexpr char kRootFolderName[] = "Linux apps";
constexpr char kDummyApp1Name[] = "dummy1";
constexpr char kDummyApp2Id[] = "dummy2";
constexpr char kDummyApp2Name[] = "dummy2";
constexpr char kAppNewName[] = "new name";
constexpr char kBananaAppId[] = "banana";
constexpr char kBananaAppName[] = "banana app name";

// Convenience matcher for some important fields of the chrome app.
MATCHER_P3(IsChromeApp, id, name, folder_id, "") {
  Matcher<std::string> id_m(id);
  Matcher<std::string> name_m(name);
  Matcher<std::string> folder_id_m(folder_id);
  return id_m.Matches(arg->id()) && name_m.Matches(arg->name()) &&
         folder_id_m.Matches(arg->folder_id());
}

// Matches a chrome app item if its persistence field is set to true.
MATCHER(IsPersistentApp, "") {
  return arg->is_persistent();
}

// For testing purposes, we want to pretend there are only crostini apps on the
// system. This method removes the others.
void RemoveNonCrostiniApps(app_list::AppListSyncableService* sync_service) {
  std::vector<std::string> existing_item_ids;
  for (const auto& pair : sync_service->sync_items()) {
    existing_item_ids.emplace_back(pair.first);
  }
  for (const std::string& id : existing_item_ids) {
    if (id == crostini::kCrostiniFolderId ||
        id == crostini::kCrostiniTerminalId) {
      continue;
    }
    sync_service->RemoveItem(id);
  }
}

}  // namespace

class CrostiniAppModelBuilderTest : public AppListTestBase {
 public:
  CrostiniAppModelBuilderTest() {}
  ~CrostiniAppModelBuilderTest() override {}

  void SetUp() override {
    AppListTestBase::SetUp();
    test_helper_ = std::make_unique<CrostiniTestHelper>(testing_profile());
    test_helper_->ReInitializeAppServiceIntegration();
    CreateBuilder();
  }

  void TearDown() override {
    ResetBuilder();
    test_helper_.reset();
    AppListTestBase::TearDown();
  }

 protected:
  AppListModelUpdater* GetModelUpdater() const {
    return sync_service_->GetModelUpdater();
  }

  size_t GetModelItemCount() const {
    // Pump the Mojo IPCs.
    base::RunLoop().RunUntilIdle();
    return sync_service_->GetModelUpdater()->ItemCount();
  }

  std::vector<ChromeAppListItem*> GetAllApps() const {
    std::vector<ChromeAppListItem*> result;
    for (size_t i = 0; i < GetModelItemCount(); ++i) {
      result.emplace_back(GetModelUpdater()->ItemAtForTest(i));
    }
    return result;
  }

  void CreateBuilder() {
    model_updater_factory_scope_ = std::make_unique<
        app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>(
        base::BindRepeating(
            [](Profile* profile) -> std::unique_ptr<AppListModelUpdater> {
              return std::make_unique<FakeAppListModelUpdater>(profile);
            },
            profile()));
    // The AppListSyncableService creates the CrostiniAppModelBuilder.
    sync_service_ =
        std::make_unique<app_list::AppListSyncableService>(profile_.get());
    RemoveNonCrostiniApps(sync_service_.get());
  }

  void ResetBuilder() {
    sync_service_.reset();
    model_updater_factory_scope_.reset();
  }

  crostini::CrostiniRegistryService* RegistryService() {
    return crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
  }

  std::string TerminalAppName() {
    return l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_NAME);
  }

  std::unique_ptr<app_list::AppListSyncableService> sync_service_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;

 private:
  std::unique_ptr<
      app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>
      model_updater_factory_scope_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppModelBuilderTest);
};

// Test that the Terminal app is only shown when Crostini is enabled
TEST_F(CrostiniAppModelBuilderTest, EnableAndDisableCrostini) {
  // Reset things so we start with Crostini not enabled.
  ResetBuilder();
  test_helper_.reset();
  test_helper_ = std::make_unique<CrostiniTestHelper>(
      testing_profile(), /*enable_crostini=*/false);
  CreateBuilder();

  EXPECT_EQ(0u, GetModelItemCount());

  CrostiniTestHelper::EnableCrostini(testing_profile());
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId)));

  CrostiniTestHelper::DisableCrostini(testing_profile());
  EXPECT_THAT(GetAllApps(), testing::IsEmpty());
}

TEST_F(CrostiniAppModelBuilderTest, AppInstallation) {
  // Terminal app.
  EXPECT_EQ(1u, GetModelItemCount());

  test_helper_->SetupDummyApps();

  EXPECT_THAT(
      GetAllApps(),
      testing::UnorderedElementsAre(
          IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                      crostini::kCrostiniFolderId),
          IsChromeApp(_, kDummyApp1Name, crostini::kCrostiniFolderId),
          IsChromeApp(_, kDummyApp2Name, crostini::kCrostiniFolderId)));

  test_helper_->AddApp(
      CrostiniTestHelper::BasicApp(kBananaAppId, kBananaAppName));
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp1Name, crostini::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp2Name, crostini::kCrostiniFolderId),
                  IsChromeApp(_, kBananaAppName, crostini::kCrostiniFolderId)));
}

// Test that the app model builder correctly picks up changes to existing apps.
TEST_F(CrostiniAppModelBuilderTest, UpdateApps) {
  test_helper_->SetupDummyApps();
  // 3 apps.
  EXPECT_EQ(3u, GetModelItemCount());

  // Setting NoDisplay to true should hide an app.
  vm_tools::apps::App dummy1 = test_helper_->GetApp(0);
  dummy1.set_no_display(true);
  test_helper_->AddApp(dummy1);
  EXPECT_THAT(
      GetAllApps(),
      testing::UnorderedElementsAre(
          IsChromeApp(crostini::kCrostiniTerminalId, _, _),
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name), _,
                      _)));

  // Setting NoDisplay to false should unhide an app.
  dummy1.set_no_display(false);
  test_helper_->AddApp(dummy1);
  EXPECT_THAT(
      GetAllApps(),
      testing::UnorderedElementsAre(
          IsChromeApp(crostini::kCrostiniTerminalId, _, _),
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp1Name), _, _),
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name), _,
                      _)));

  // Changes to app names should be detected.
  vm_tools::apps::App dummy2 =
      CrostiniTestHelper::BasicApp(kDummyApp2Id, kAppNewName);
  test_helper_->AddApp(dummy2);
  EXPECT_THAT(
      GetAllApps(),
      testing::UnorderedElementsAre(
          IsChromeApp(crostini::kCrostiniTerminalId, _, _),
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp1Name),
                      kDummyApp1Name, _),
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name),
                      kAppNewName, _)));
}

// Test that the app model builder handles removed apps
TEST_F(CrostiniAppModelBuilderTest, RemoveApps) {
  test_helper_->SetupDummyApps();
  // 3 apps.
  EXPECT_EQ(3u, GetModelItemCount());

  // Remove dummy1
  test_helper_->RemoveApp(0);
  EXPECT_EQ(2u, GetModelItemCount());

  // Remove dummy2
  test_helper_->RemoveApp(0);
  EXPECT_EQ(1u, GetModelItemCount());
}

// Tests that the crostini folder is (re)created with the correct parameters.
TEST_F(CrostiniAppModelBuilderTest, CreatesFolder) {
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId)));

  // We simulate ash creating the crostini folder and calling back into chrome
  // (rather than use a full browser test).
  auto metadata = std::make_unique<ash::AppListItemMetadata>();
  metadata->id = crostini::kCrostiniFolderId;
  GetModelUpdater()->OnFolderCreated(std::move(metadata));

  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId),
                  testing::AllOf(IsChromeApp(crostini::kCrostiniFolderId,
                                             kRootFolderName, ""),
                                 IsPersistentApp())));
}

// Test that the Terminal app is removed when Crostini is disabled.
TEST_F(CrostiniAppModelBuilderTest, DisableCrostini) {
  test_helper_->SetupDummyApps();
  // 3 apps.
  EXPECT_EQ(3u, GetModelItemCount());

  // The uninstall flow removes all apps before setting the CrostiniEnabled pref
  // to false, so we need to do that explicitly too.
  RegistryService()->ClearApplicationList(crostini::kCrostiniDefaultVmName, "");
  CrostiniTestHelper::DisableCrostini(testing_profile());
  EXPECT_EQ(0u, GetModelItemCount());
}
