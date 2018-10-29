// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_arc_home_service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_model_builder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/arc/arc_fast_app_reinstall_starter.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_pai_starter.h"
#include "chrome/browser/ui/app_list/arc/mock_arc_app_list_prefs_observer.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr char kTestPackageName[] = "fake.package.name2";

class FakeAppIconLoaderDelegate : public AppIconLoaderDelegate {
 public:
  FakeAppIconLoaderDelegate() = default;
  ~FakeAppIconLoaderDelegate() override = default;

  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override {
    app_id_ = app_id;
    image_ = image;
    ++update_image_count_;
    if (update_image_count_ == expected_update_image_count_ &&
        !icon_updated_callback_.is_null()) {
      base::ResetAndReturn(&icon_updated_callback_).Run();
    }
  }

  void WaitForIconUpdates(size_t expected_updates) {
    base::RunLoop run_loop;
    expected_update_image_count_ = expected_updates + update_image_count_;
    icon_updated_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  size_t update_image_count() const { return update_image_count_; }

  const std::string& app_id() const { return app_id_; }

  const gfx::ImageSkia& image() { return image_; }

 private:
  size_t update_image_count_ = 0;
  size_t expected_update_image_count_ = 0;
  std::string app_id_;
  gfx::ImageSkia image_;
  base::OnceClosure icon_updated_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppIconLoaderDelegate);
};

ArcAppIconDescriptor GetAppListIconDescriptor(ui::ScaleFactor scale_factor) {
  return ArcAppIconDescriptor(
      app_list::AppListConfig::instance().grid_icon_dimension(), scale_factor);
}

bool IsIconCreated(ArcAppListPrefs* prefs,
                   const std::string& app_id,
                   ui::ScaleFactor scale_factor) {
  return base::PathExists(
      prefs->GetIconPath(app_id, GetAppListIconDescriptor(scale_factor)));
}

void WaitForIconCreation(ArcAppListPrefs* prefs,
                         const std::string& app_id,
                         ui::ScaleFactor scale_factor) {
  const base::FilePath icon_path =
      prefs->GetIconPath(app_id, GetAppListIconDescriptor(scale_factor));
  // Process pending tasks. This performs multiple thread hops, so we need
  // to run it continuously until it is resolved.
  do {
    content::RunAllTasksUntilIdle();
  } while (!base::PathExists(icon_path));
}

void WaitForIconUpdates(Profile* profile,
                        const std::string& app_id,
                        size_t expected_updates) {
  FakeAppIconLoaderDelegate delegate;
  ArcAppIconLoader icon_loader(
      profile, app_list::AppListConfig::instance().grid_icon_dimension(),
      &delegate);
  icon_loader.FetchImage(app_id);
  delegate.WaitForIconUpdates(expected_updates);
}

enum class ArcState {
  // By default, ARC is non-persistent and Play Store is unmanaged.
  ARC_PLAY_STORE_UNMANAGED,
  // ARC is persistent and Play Store is unmanaged
  ARC_PERSISTENT_PLAY_STORE_UNMANAGED,
  // ARC is non-persistent and Play Store is managed and enabled.
  ARC_PLAY_STORE_MANAGED_AND_ENABLED,
  // ARC is non-persistent and Play Store is managed and disabled.
  ARC_PLAY_STORE_MANAGED_AND_DISABLED,
  // ARC is persistent and Play Store is managed and enabled.
  ARC_PERSISTENT_PLAY_STORE_MANAGED_AND_ENABLED,
  // ARC is persistent and Play Store is managed and disabled.
  ARC_PERSISTENT_PLAY_STORE_MANAGED_AND_DISABLED,
  // ARC is persistent but without Play Store UI support.
  ARC_PERSISTENT_WITHOUT_PLAY_STORE,
  // ARC is persistent, Play Store is managed, enabled, but hidden.
  ARC_PERSISTENT_MANAGED_ENABLED_AND_PLAY_STORE_HIDDEN,
};

constexpr ArcState kManagedArcStates[] = {
    ArcState::ARC_PLAY_STORE_MANAGED_AND_ENABLED,
    ArcState::ARC_PLAY_STORE_MANAGED_AND_DISABLED,
    ArcState::ARC_PERSISTENT_PLAY_STORE_MANAGED_AND_ENABLED,
    ArcState::ARC_PERSISTENT_PLAY_STORE_MANAGED_AND_DISABLED,
    ArcState::ARC_PERSISTENT_MANAGED_ENABLED_AND_PLAY_STORE_HIDDEN,
};

constexpr ArcState kUnmanagedArcStates[] = {
    ArcState::ARC_PLAY_STORE_UNMANAGED,
    ArcState::ARC_PERSISTENT_PLAY_STORE_UNMANAGED,
    ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE,
};

constexpr ArcState kUnmanagedArcStatesWithPlayStore[] = {
    ArcState::ARC_PLAY_STORE_UNMANAGED,
    ArcState::ARC_PERSISTENT_PLAY_STORE_UNMANAGED,
};

void OnPaiStartedCallback(bool* started_flag) {
  *started_flag = true;
}

int GetAppListIconDimensionForScaleFactor(ui::ScaleFactor scale_factor) {
  switch (scale_factor) {
    case ui::SCALE_FACTOR_100P:
      return app_list::AppListConfig::instance().grid_icon_dimension();
    case ui::SCALE_FACTOR_200P:
      return app_list::AppListConfig::instance().grid_icon_dimension() * 2;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

class ArcAppModelBuilderTest : public extensions::ExtensionServiceTestBase,
                               public ::testing::WithParamInterface<ArcState> {
 public:
  ArcAppModelBuilderTest() = default;
  ~ArcAppModelBuilderTest() override {
    // Release profile file in order to keep right sequence.
    profile_.reset();
  }

  void SetUp() override {
    switch (GetParam()) {
      case ArcState::ARC_PERSISTENT_PLAY_STORE_UNMANAGED:
      case ArcState::ARC_PERSISTENT_PLAY_STORE_MANAGED_AND_ENABLED:
      case ArcState::ARC_PERSISTENT_PLAY_STORE_MANAGED_AND_DISABLED:
      case ArcState::ARC_PERSISTENT_MANAGED_ENABLED_AND_PLAY_STORE_HIDDEN:
        arc::SetArcAlwaysStartForTesting(true);
        break;
      case ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE:
        arc::SetArcAlwaysStartForTesting(false);
        break;
      default:
        break;
    }

    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service_->Init();

    OnBeforeArcTestSetup();
    arc_test_.SetUp(profile_.get());
    CreateBuilder();

    // Validating decoded content does not fit well for unit tests.
    ArcAppIcon::DisableSafeDecodingForTesting();
  }

  void TearDown() override {
    arc_test_.TearDown();
    ResetBuilder();
    extensions::ExtensionServiceTestBase::TearDown();
  }

 protected:
  // Notifies that initial preparation is done, profile is ready and it is time
  // to initialize ARC subsystem.
  virtual void OnBeforeArcTestSetup() {}

  // Creates a new builder, destroying any existing one.
  void CreateBuilder() {
    ResetBuilder();  // Destroy any existing builder in the correct order.

    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ = std::make_unique<ArcAppModelBuilder>(controller_.get());
    builder_->Initialize(nullptr, profile_.get(), model_updater_.get());
  }

  void ResetBuilder() {
    builder_.reset();
    controller_.reset();
    model_updater_.reset();
  }

  size_t GetArcItemCount() const {
    size_t arc_count = 0;
    const size_t count = model_updater_->ItemCount();
    for (size_t i = 0; i < count; ++i) {
      ChromeAppListItem* item = model_updater_->ItemAtForTest(i);
      if (item->GetItemType() == ArcAppItem::kItemType)
        ++arc_count;
    }
    return arc_count;
  }

  ArcAppItem* GetArcItem(size_t index) const {
    size_t arc_count = 0;
    const size_t count = model_updater_->ItemCount();
    ArcAppItem* arc_item = nullptr;
    for (size_t i = 0; i < count; ++i) {
      ChromeAppListItem* item = model_updater_->ItemAtForTest(i);
      if (item->GetItemType() == ArcAppItem::kItemType) {
        if (arc_count++ == index) {
          arc_item = reinterpret_cast<ArcAppItem*>(item);
          break;
        }
      }
    }
    EXPECT_NE(nullptr, arc_item);
    return arc_item;
  }

  ArcAppItem* FindArcItem(const std::string& id) const {
    const size_t count = GetArcItemCount();
    ArcAppItem* found_item = nullptr;
    for (size_t i = 0; i < count; ++i) {
      ArcAppItem* item = GetArcItem(i);
      if (item && item->id() == id) {
        DCHECK(!found_item);
        found_item = item;
      }
    }
    return found_item;
  }

  // Validate that prefs and model have right content.
  void ValidateHaveApps(const std::vector<arc::mojom::AppInfo> apps) {
    ValidateHaveAppsAndShortcuts(apps, std::vector<arc::mojom::ShortcutInfo>());
  }

  void ValidateHaveShortcuts(
      const std::vector<arc::mojom::ShortcutInfo> shortcuts) {
    ValidateHaveAppsAndShortcuts(std::vector<arc::mojom::AppInfo>(), shortcuts);
  }

  void ValidateHaveAppsAndShortcuts(
      const std::vector<arc::mojom::AppInfo> apps,
      const std::vector<arc::mojom::ShortcutInfo> shortcuts) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);
    const std::vector<std::string> ids = prefs->GetAppIds();
    ASSERT_EQ(apps.size() + shortcuts.size(), ids.size());
    ASSERT_EQ(apps.size() + shortcuts.size(), GetArcItemCount());
    // In principle, order of items is not defined.
    for (const auto& app : apps) {
      const std::string id = ArcAppTest::GetAppId(app);
      EXPECT_TRUE(base::ContainsValue(ids, id));
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(app.name, app_info->name);
      EXPECT_EQ(app.package_name, app_info->package_name);
      EXPECT_EQ(app.activity, app_info->activity);

      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_EQ(app.name, app_item->name());
    }

    for (auto& shortcut : shortcuts) {
      const std::string id = ArcAppTest::GetAppId(shortcut);
      EXPECT_TRUE(base::ContainsValue(ids, id));
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(shortcut.name, app_info->name);
      EXPECT_EQ(shortcut.package_name, app_info->package_name);
      EXPECT_EQ(shortcut.intent_uri, app_info->intent_uri);

      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_EQ(shortcut.name, app_item->name());
    }
  }

  // Validate that prefs have right packages.
  void ValidateHavePackages(
      const std::vector<arc::mojom::ArcPackageInfo> packages) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);
    const std::vector<std::string> pref_packages =
        prefs->GetPackagesFromPrefs();
    ASSERT_EQ(packages.size(), pref_packages.size());
    for (const auto& package : packages) {
      const std::string package_name = package.package_name;
      std::unique_ptr<ArcAppListPrefs::PackageInfo> package_info =
          prefs->GetPackage(package_name);
      ASSERT_NE(nullptr, package_info.get());
      EXPECT_EQ(package.last_backup_android_id,
                package_info->last_backup_android_id);
      EXPECT_EQ(package.last_backup_time, package_info->last_backup_time);
      EXPECT_EQ(package.sync, package_info->should_sync);
    }
  }

  // Validate that requested apps have required ready state and other apps have
  // opposite state.
  void ValidateAppReadyState(const std::vector<arc::mojom::AppInfo> apps,
                             bool ready) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);

    std::vector<std::string> ids = prefs->GetAppIds();
    EXPECT_EQ(ids.size(), GetArcItemCount());

    // Process requested apps.
    for (auto& app : apps) {
      const std::string id = ArcAppTest::GetAppId(app);
      std::vector<std::string>::iterator it_id =
          std::find(ids.begin(), ids.end(), id);
      ASSERT_NE(it_id, ids.end());
      ids.erase(it_id);

      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(ready, app_info->ready);
    }

    // Process the rest of the apps.
    for (auto& id : ids) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_NE(ready, app_info->ready);
    }
  }

  // Validate that requested shortcuts have required ready state
  void ValidateShortcutReadyState(
      const std::vector<arc::mojom::ShortcutInfo> shortcuts,
      bool ready) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);

    std::vector<std::string> ids = prefs->GetAppIds();
    EXPECT_EQ(ids.size(), GetArcItemCount());

    // Process requested apps.
    for (auto& shortcut : shortcuts) {
      const std::string id = ArcAppTest::GetAppId(shortcut);
      std::vector<std::string>::iterator it_id =
          std::find(ids.begin(), ids.end(), id);
      ASSERT_NE(it_id, ids.end());
      ids.erase(it_id);

      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(ready, app_info->ready);
    }
  }

  // Validates that provided image is acceptable as ARC app icon.
  void ValidateIcon(const gfx::ImageSkia& image) {
    const int icon_dimension =
        app_list::AppListConfig::instance().grid_icon_dimension();
    EXPECT_EQ(icon_dimension, image.width());
    EXPECT_EQ(icon_dimension, image.height());

    const std::vector<ui::ScaleFactor>& scale_factors =
        ui::GetSupportedScaleFactors();
    for (auto& scale_factor : scale_factors) {
      const float scale = ui::GetScaleForScaleFactor(scale_factor);
      EXPECT_TRUE(image.HasRepresentation(scale));
      const gfx::ImageSkiaRep& representation = image.GetRepresentation(scale);
      EXPECT_FALSE(representation.is_null());
      EXPECT_EQ(gfx::ToCeiledInt(icon_dimension * scale),
                representation.pixel_width());
      EXPECT_EQ(gfx::ToCeiledInt(icon_dimension * scale),
                representation.pixel_height());
    }
  }

  // Removes icon request record and allowd re-sending icon request.
  void MaybeRemoveIconRequestRecord(const std::string& app_id) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);

    prefs->MaybeRemoveIconRequestRecord(app_id);
  }

  void AddPackage(const arc::mojom::ArcPackageInfo& package) {
    arc_test_.AddPackage(package);
    app_instance()->SendPackageAdded(package);
  }

  void RemovePackage(const arc::mojom::ArcPackageInfo& package) {
    arc_test_.RemovePackage(package);
    app_instance()->SendPackageUninstalled(package.package_name);
  }

  AppListControllerDelegate* controller() { return controller_.get(); }

  TestingProfile* profile() { return profile_.get(); }

  ArcAppTest* arc_test() { return &arc_test_; }

  const std::vector<arc::mojom::AppInfo>& fake_apps() const {
    return arc_test_.fake_apps();
  }

  const std::vector<arc::mojom::AppInfo>& fake_default_apps() const {
    return arc_test_.fake_default_apps();
  }

  const std::vector<arc::mojom::ArcPackageInfo>& fake_packages() const {
    return arc_test_.fake_packages();
  }

  const std::vector<arc::mojom::ShortcutInfo>& fake_shortcuts() const {
    return arc_test_.fake_shortcuts();
  }

  arc::FakeAppInstance* app_instance() { return arc_test_.app_instance(); }

 private:
  ArcAppTest arc_test_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<ArcAppModelBuilder> builder_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppModelBuilderTest);
};

class ArcAppModelBuilderRecreate : public ArcAppModelBuilderTest {
 public:
  ArcAppModelBuilderRecreate() = default;
  ~ArcAppModelBuilderRecreate() override = default;

 protected:
  // Simulates ARC restart.
  void RestartArc() {
    arc_test()->TearDown();
    ResetBuilder();

    ArcAppListPrefsFactory::GetInstance()->RecreateServiceInstanceForTesting(
        profile_.get());
    arc_test()->SetUp(profile_.get());
    CreateBuilder();
  }

  // ArcAppModelBuilderTest:
  void OnBeforeArcTestSetup() override {
    arc::ArcPackageSyncableServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), BrowserContextKeyedServiceFactory::TestingFactory());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppModelBuilderRecreate);
};

class ArcDefaulAppTest : public ArcAppModelBuilderRecreate {
 public:
  ArcDefaulAppTest() = default;
  ~ArcDefaulAppTest() override = default;

 protected:
  // ArcAppModelBuilderTest:
  void OnBeforeArcTestSetup() override {
    ArcDefaultAppList::UseTestAppsDirectory();
    arc_test()->set_wait_default_apps(IsWaitDefaultAppsNeeded());
    ArcAppModelBuilderRecreate::OnBeforeArcTestSetup();
  }

  // Returns true if test needs to wait for default apps on setup.
  virtual bool IsWaitDefaultAppsNeeded() const { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDefaulAppTest);
};

class ArcAppLauncherForDefaulAppTest : public ArcDefaulAppTest {
 public:
  ArcAppLauncherForDefaulAppTest() = default;
  ~ArcAppLauncherForDefaulAppTest() override = default;

 protected:
  // ArcDefaulAppTest:
  bool IsWaitDefaultAppsNeeded() const override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppLauncherForDefaulAppTest);
};

class ArcPlayStoreAppTest : public ArcDefaulAppTest {
 public:
  ArcPlayStoreAppTest() = default;
  ~ArcPlayStoreAppTest() override = default;

 protected:
  // ArcAppModelBuilderTest:
  void OnBeforeArcTestSetup() override {
    ArcDefaulAppTest::OnBeforeArcTestSetup();

    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName, "Play Store");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
    manifest.SetString(extensions::manifest_keys::kDescription,
                       "Play Store for testing");

    std::string error;
    arc_support_host_ = extensions::Extension::Create(
        base::FilePath(), extensions::Manifest::UNPACKED, manifest,
        extensions::Extension::NO_FLAGS, arc::kPlayStoreAppId, &error);

    extensions::ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile_.get())->extension_service();
    extension_service->AddExtension(arc_support_host_.get());
  }

  void SendPlayStoreApp() {
    arc::mojom::AppInfo app;
    app.name = "Play Store";
    app.package_name = arc::kPlayStorePackage;
    app.activity = arc::kPlayStoreActivity;
    app.sticky = GetParam() != ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE;

    app_instance()->RefreshAppList();
    app_instance()->SendRefreshAppList({app});
  }

 private:
  scoped_refptr<extensions::Extension> arc_support_host_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreAppTest);
};

class ArcDefaulAppForManagedUserTest : public ArcPlayStoreAppTest {
 public:
  ArcDefaulAppForManagedUserTest() = default;
  ~ArcDefaulAppForManagedUserTest() override = default;

 protected:
  bool IsEnabledByPolicy() const {
    switch (GetParam()) {
      case ArcState::ARC_PLAY_STORE_MANAGED_AND_ENABLED:
      case ArcState::ARC_PERSISTENT_PLAY_STORE_MANAGED_AND_ENABLED:
      case ArcState::ARC_PERSISTENT_MANAGED_ENABLED_AND_PLAY_STORE_HIDDEN:
        return true;
      case ArcState::ARC_PLAY_STORE_MANAGED_AND_DISABLED:
      case ArcState::ARC_PERSISTENT_PLAY_STORE_MANAGED_AND_DISABLED:
      case ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE:
        return false;
      default:
        NOTREACHED();
        return false;
    }
  }

  // ArcPlayStoreAppTest:
  void OnBeforeArcTestSetup() override {
    if (GetParam() ==
        ArcState::ARC_PERSISTENT_MANAGED_ENABLED_AND_PLAY_STORE_HIDDEN) {
      const AccountId account_id(
          AccountId::FromUserEmail(profile_->GetProfileUserName()));
      arc_test()->GetUserManager()->AddPublicAccountUser(account_id);
      arc_test()->GetUserManager()->LoginUser(account_id);
    }
    policy::ProfilePolicyConnector* const connector =
        policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile());
    connector->OverrideIsManagedForTesting(true);
    profile()->GetTestingPrefService()->SetManagedPref(
        arc::prefs::kArcEnabled,
        std::make_unique<base::Value>(IsEnabledByPolicy()));

    ArcPlayStoreAppTest::OnBeforeArcTestSetup();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDefaulAppForManagedUserTest);
};

class ArcVoiceInteractionTest : public ArcPlayStoreAppTest {
 public:
  ArcVoiceInteractionTest() = default;
  ~ArcVoiceInteractionTest() override = default;

  void SetUp() override {
    ArcPlayStoreAppTest::SetUp();

    arc::ArcSessionManager* session_manager = arc::ArcSessionManager::Get();
    DCHECK(session_manager);

    pai_starter_ = session_manager->pai_starter();
    DCHECK(pai_starter_);
    DCHECK(!pai_starter_->started());
    DCHECK(!pai_starter_->locked());

    voice_service_ = std::make_unique<arc::ArcVoiceInteractionArcHomeService>(
        profile(), arc::ArcServiceManager::Get()->arc_bridge_service());
    voice_service()->OnAssistantStarted();

    SendPlayStoreApp();

    DCHECK(!pai_starter_->started());
    DCHECK(pai_starter_->locked());
  }

  void TearDown() override {
    ArcPlayStoreAppTest::TearDown();
    voice_service_.reset();
  }

 protected:
  void SendAssistantAppStarted() {
    arc::mojom::AppInfo app;
    app.name = "Assistant";
    app.package_name =
        arc::ArcVoiceInteractionArcHomeService::kAssistantPackageName;
    app.activity = "some_activity";

    app_instance()->SendTaskCreated(1, app, std::string());
  }

  void SendAssistantAppStopped() { app_instance()->SendTaskDestroyed(1); }

  void WaitForPaiStarted() {
    while (!pai_starter()->started())
      base::RunLoop().RunUntilIdle();
  }

  arc::ArcVoiceInteractionArcHomeService* voice_service() {
    return voice_service_.get();
  }
  arc::ArcPaiStarter* pai_starter() { return pai_starter_; }

 private:
  arc::ArcPaiStarter* pai_starter_ = nullptr;
  std::unique_ptr<arc::ArcVoiceInteractionArcHomeService> voice_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcVoiceInteractionTest);
};

TEST_P(ArcAppModelBuilderTest, ArcPackagePref) {
  ValidateHavePackages(std::vector<arc::mojom::ArcPackageInfo>());
  app_instance()->SendRefreshPackageList(fake_packages());
  ValidateHavePackages(fake_packages());

  arc::mojom::ArcPackageInfo package;
  package.package_name = kTestPackageName;
  package.package_version = 2;
  package.last_backup_android_id = 2;
  package.last_backup_time = 2;
  package.sync = true;

  RemovePackage(package);
  ValidateHavePackages(fake_packages());

  AddPackage(package);
  ValidateHavePackages(fake_packages());
}

TEST_P(ArcAppModelBuilderTest, RefreshAllOnReady) {
  // There should already have been one call, when the interface was
  // registered.
  EXPECT_EQ(1, app_instance()->refresh_app_list_count());
  app_instance()->RefreshAppList();
  EXPECT_EQ(2, app_instance()->refresh_app_list_count());
}

TEST_P(ArcAppModelBuilderTest, RefreshAllFillsContent) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveApps(fake_apps());
}

TEST_P(ArcAppModelBuilderTest, InstallUninstallShortcut) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());

  std::vector<arc::mojom::ShortcutInfo> shortcuts = fake_shortcuts();
  ASSERT_GE(shortcuts.size(), 2U);

  // Adding package is required to safely call SendPackageUninstalled.
  arc::mojom::ArcPackageInfo package;
  package.package_name = shortcuts[1].package_name;
  package.package_version = 1;
  package.sync = true;
  AddPackage(package);

  app_instance()->SendInstallShortcuts(shortcuts);
  ValidateHaveShortcuts(shortcuts);

  // Uninstall first shortcut and validate it was removed.
  const std::string package_name = shortcuts[0].package_name;
  const std::string intent_uri = shortcuts[0].intent_uri;
  shortcuts.erase(shortcuts.begin());
  app_instance()->SendUninstallShortcut(package_name, intent_uri);
  ValidateHaveShortcuts(shortcuts);

  // Requests to uninstall non-existing shortcuts should be just ignored.
  EXPECT_NE(package_name, shortcuts[0].package_name);
  EXPECT_NE(intent_uri, shortcuts[0].intent_uri);
  app_instance()->SendUninstallShortcut(package_name, shortcuts[0].intent_uri);
  app_instance()->SendUninstallShortcut(shortcuts[0].package_name, intent_uri);
  ValidateHaveShortcuts(shortcuts);

  // Removing package should also remove associated shortcuts.
  app_instance()->SendPackageUninstalled(shortcuts[0].package_name);
  shortcuts.erase(shortcuts.begin());
  ValidateHaveShortcuts(shortcuts);
}

TEST_P(ArcAppModelBuilderTest, RefreshAllPreservesShortcut) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveApps(fake_apps());

  app_instance()->SendInstallShortcuts(fake_shortcuts());
  ValidateHaveAppsAndShortcuts(fake_apps(), fake_shortcuts());

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveAppsAndShortcuts(fake_apps(), fake_shortcuts());
}

TEST_P(ArcAppModelBuilderTest, MultipleRefreshAll) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  app_instance()->RefreshAppList();
  // Send info about all fake apps except last.
  std::vector<arc::mojom::AppInfo> apps1(fake_apps().begin(),
                                         fake_apps().end() - 1);
  app_instance()->SendRefreshAppList(apps1);
  // At this point all apps (except last) should exist and be ready.
  ValidateHaveApps(apps1);
  ValidateAppReadyState(apps1, true);

  // Send info about all fake apps except first.
  std::vector<arc::mojom::AppInfo> apps2(fake_apps().begin() + 1,
                                         fake_apps().end());
  app_instance()->SendRefreshAppList(apps2);
  // At this point all apps should exist but first one should be non-ready.
  ValidateHaveApps(apps2);
  ValidateAppReadyState(apps2, true);

  // Send info about all fake apps.
  app_instance()->SendRefreshAppList(fake_apps());
  // At this point all apps should exist and be ready.
  ValidateHaveApps(fake_apps());
  ValidateAppReadyState(fake_apps(), true);

  // Send info no app available.
  std::vector<arc::mojom::AppInfo> no_apps;
  app_instance()->SendRefreshAppList(no_apps);
  // At this point no app should exist.
  ValidateHaveApps(no_apps);
}

TEST_P(ArcAppModelBuilderTest, StopStartServicePreserveApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->RefreshAppList();
  EXPECT_EQ(0u, GetArcItemCount());
  EXPECT_EQ(0u, prefs->GetAppIds().size());

  app_instance()->SendRefreshAppList(fake_apps());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_apps().size(), ids.size());
  ValidateAppReadyState(fake_apps(), true);

  // Stopping service does not delete items. It makes them non-ready.
  arc_test()->StopArcInstance();
  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), false);

  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), false);

  // Refreshing app list makes apps available.
  arc_test()->RestartArcInstance();
  app_instance()->SendRefreshAppList(fake_apps());
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), true);
}

TEST_P(ArcAppModelBuilderTest, StopStartServicePreserveShortcuts) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->RefreshAppList();
  EXPECT_EQ(0u, GetArcItemCount());
  EXPECT_EQ(0u, prefs->GetAppIds().size());

  app_instance()->SendInstallShortcuts(fake_shortcuts());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_shortcuts().size(), ids.size());
  ValidateShortcutReadyState(fake_shortcuts(), true);

  // Stopping service does not delete items. It makes them non-ready.
  arc_test()->StopArcInstance();
  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateShortcutReadyState(fake_shortcuts(), false);

  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateShortcutReadyState(fake_shortcuts(), false);

  // Refreshing app list makes apps available.
  arc_test()->RestartArcInstance();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>());
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateShortcutReadyState(fake_shortcuts(), true);
}

TEST_P(ArcAppModelBuilderTest, RestartPreserveApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  // Start from scratch and fill with apps.
  app_instance()->SendRefreshAppList(fake_apps());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_apps().size(), ids.size());
  ValidateAppReadyState(fake_apps(), true);

  // This recreates model and ARC apps will be read from prefs.
  arc_test()->StopArcInstance();
  CreateBuilder();
  ValidateAppReadyState(fake_apps(), false);
}

TEST_P(ArcAppModelBuilderTest, RestartPreserveShortcuts) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  // Start from scratch and install shortcuts.
  app_instance()->SendInstallShortcuts(fake_shortcuts());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_apps().size(), ids.size());
  ValidateShortcutReadyState(fake_shortcuts(), true);

  // This recreates model and ARC apps and shortcuts will be read from prefs.
  arc_test()->StopArcInstance();
  CreateBuilder();
  ValidateShortcutReadyState(fake_shortcuts(), false);
}

TEST_P(ArcAppModelBuilderTest, LaunchApps) {
  // Disable attempts to dismiss app launcher view.
  ChromeAppListItem::OverrideAppListControllerDelegateForTesting(controller());

  std::vector<arc::mojom::AppInfo> apps = fake_apps();
  ASSERT_GE(apps.size(), 3U);

  apps[2].suspended = true;

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps);

  // Simulate item activate.
  ArcAppItem* item1 = FindArcItem(ArcAppTest::GetAppId(apps[0]));
  ArcAppItem* item2 = FindArcItem(ArcAppTest::GetAppId(apps[1]));
  ArcAppItem* item3 = FindArcItem(ArcAppTest::GetAppId(apps[2]));
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  item1->PerformActivate(0);
  item2->PerformActivate(0);
  item1->PerformActivate(0);

  const std::vector<std::unique_ptr<arc::FakeAppInstance::Request>>&
      launch_requests = app_instance()->launch_requests();
  ASSERT_EQ(3u, launch_requests.size());
  EXPECT_TRUE(launch_requests[0]->IsForApp(apps[0]));
  EXPECT_TRUE(launch_requests[1]->IsForApp(apps[1]));
  EXPECT_TRUE(launch_requests[2]->IsForApp(apps[0]));

  // Test an attempt to launch suspended app. It should be blocked.
  item3->PerformActivate(0);
  EXPECT_EQ(3u, app_instance()->launch_requests().size());

  // Test an attempt to launch of a not-ready app. Number of launch requests
  // should be the same, indicating that launch request was blocked.
  arc_test()->StopArcInstance();
  item1 = FindArcItem(ArcAppTest::GetAppId(apps[0]));
  ASSERT_TRUE(item1);
  item1->PerformActivate(0);
  EXPECT_EQ(3u, app_instance()->launch_requests().size());
}

TEST_P(ArcAppModelBuilderTest, LaunchShortcuts) {
  // Disable attempts to dismiss app launcher view.
  ChromeAppListItem::OverrideAppListControllerDelegateForTesting(controller());

  app_instance()->RefreshAppList();
  app_instance()->SendInstallShortcuts(fake_shortcuts());

  // Simulate item activate.
  ASSERT_GE(fake_shortcuts().size(), 2U);
  const arc::mojom::ShortcutInfo& app_first = fake_shortcuts()[0];
  const arc::mojom::ShortcutInfo& app_last = fake_shortcuts()[1];
  ArcAppItem* item_first = FindArcItem(ArcAppTest::GetAppId(app_first));
  ArcAppItem* item_last = FindArcItem(ArcAppTest::GetAppId(app_last));
  ASSERT_NE(nullptr, item_first);
  ASSERT_NE(nullptr, item_last);
  item_first->PerformActivate(0);
  item_last->PerformActivate(0);
  item_first->PerformActivate(0);

  const std::vector<std::string>& launch_intents =
      app_instance()->launch_intents();
  ASSERT_EQ(3u, launch_intents.size());
  EXPECT_EQ(app_first.intent_uri, launch_intents[0]);
  EXPECT_EQ(app_last.intent_uri, launch_intents[1]);
  EXPECT_EQ(app_first.intent_uri, launch_intents[2]);

  // Test an attempt to launch of a not-ready shortcut.
  arc_test()->StopArcInstance();
  item_first = FindArcItem(ArcAppTest::GetAppId(app_first));
  ASSERT_NE(nullptr, item_first);
  size_t launch_request_count_before = app_instance()->launch_intents().size();
  item_first->PerformActivate(0);
  // Number of launch requests must not change.
  EXPECT_EQ(launch_request_count_before,
            app_instance()->launch_intents().size());
}

TEST_P(ArcAppModelBuilderTest, RequestIcons) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());

  // Validate that no icon exists at the beginning and request icon for
  // each supported scale factor. This will start asynchronous loading.
  std::set<int> expected_dimensions;
  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  for (auto& scale_factor : scale_factors) {
    expected_dimensions.insert(
        GetAppListIconDimensionForScaleFactor(scale_factor));
    for (auto& app : fake_apps()) {
      ArcAppItem* app_item = FindArcItem(ArcAppTest::GetAppId(app));
      ASSERT_NE(nullptr, app_item);
      const float scale = ui::GetScaleForScaleFactor(scale_factor);
      app_item->icon().GetRepresentation(scale);

      // This does not result in an icon being loaded, so WaitForIconUpdates
      // cannot be used.
      content::RunAllTasksUntilIdle();
    }
  }

  const size_t expected_size = scale_factors.size() * fake_apps().size();

  // At this moment we should receive all requests for icon loading.
  const std::vector<std::unique_ptr<arc::FakeAppInstance::IconRequest>>&
      icon_requests = app_instance()->icon_requests();
  EXPECT_EQ(expected_size, icon_requests.size());
  std::map<std::string, std::set<int>> app_dimensions;
  for (size_t i = 0; i < icon_requests.size(); ++i) {
    const arc::FakeAppInstance::IconRequest* icon_request =
        icon_requests[i].get();
    const std::string id = ArcAppListPrefs::GetAppId(
        icon_request->package_name(), icon_request->activity());
    // Make sure no double requests. Dimension is stepped by 16.
    EXPECT_EQ(0U, app_dimensions[id].count(icon_request->dimension()));
    app_dimensions[id].insert(icon_request->dimension());
  }

  // Validate that we have a request for each icon for each supported scale
  // factor.
  EXPECT_EQ(fake_apps().size(), app_dimensions.size());
  for (auto& app : fake_apps()) {
    const std::string id = ArcAppTest::GetAppId(app);
    ASSERT_NE(app_dimensions.find(id), app_dimensions.end());
    EXPECT_EQ(app_dimensions[id], expected_dimensions);
  }
}

TEST_P(ArcAppModelBuilderTest, RequestShortcutIcons) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  const arc::mojom::ShortcutInfo& shortcut = fake_shortcuts()[0];
  app_instance()->SendInstallShortcut(shortcut);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  // Icons representations loading is done asynchronously and is started once
  // the ArcAppItem is created. Wait for icons for all supported scales to be
  // loaded.
  std::set<int> expected_dimensions;
  ArcAppItem* app_item = FindArcItem(ArcAppTest::GetAppId(shortcut));
  ASSERT_NE(nullptr, app_item);
  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  WaitForIconUpdates(profile_.get(), app_item->id(), scale_factors.size());
  for (auto& scale_factor : scale_factors) {
    expected_dimensions.insert(
        GetAppListIconDimensionForScaleFactor(scale_factor));
    EXPECT_TRUE(
        IsIconCreated(prefs, ArcAppTest::GetAppId(shortcut), scale_factor));
  }

  // At this moment we should receive all requests for icon loading.
  const size_t expected_size = scale_factors.size();
  const std::vector<std::unique_ptr<arc::FakeAppInstance::ShortcutIconRequest>>&
      icon_requests = app_instance()->shortcut_icon_requests();
  EXPECT_EQ(expected_size, icon_requests.size());
  std::set<int> shortcut_dimensions;
  for (size_t i = 0; i < icon_requests.size(); ++i) {
    const arc::FakeAppInstance::ShortcutIconRequest* icon_request =
        icon_requests[i].get();
    EXPECT_EQ(shortcut.icon_resource_id, icon_request->icon_resource_id());

    // Make sure no double requests.
    EXPECT_EQ(0U, shortcut_dimensions.count(icon_request->dimension()));
    shortcut_dimensions.insert(icon_request->dimension());
  }

  // Validate that we have a request for each icon for each supported scale
  // factor.
  EXPECT_EQ(shortcut_dimensions, expected_dimensions);

  // Validate all icon files are installed.
  for (auto& scale_factor : scale_factors) {
    const base::FilePath icon_path = prefs->GetIconPath(
        ArcAppTest::GetAppId(shortcut), GetAppListIconDescriptor(scale_factor));
    EXPECT_TRUE(base::PathExists(icon_path));
  }
}

TEST_P(ArcAppModelBuilderTest, InstallIcon) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));
  const arc::mojom::AppInfo& app = fake_apps()[0];

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors()[0];
  const float scale = ui::GetScaleForScaleFactor(scale_factor);
  const std::string app_id = ArcAppTest::GetAppId(app);
  const base::FilePath icon_path =
      prefs->GetIconPath(app_id, GetAppListIconDescriptor(scale_factor));
  EXPECT_FALSE(IsIconCreated(prefs, app_id, scale_factor));

  const ArcAppItem* app_item = FindArcItem(app_id);
  EXPECT_NE(nullptr, app_item);
  // This initiates async loading.
  app_item->icon().GetRepresentation(scale);

  WaitForIconUpdates(profile_.get(), app_id, 1);

  // Validate that icons are installed, have right content and icon is
  // refreshed for ARC app item.
  EXPECT_TRUE(IsIconCreated(prefs, app_id, scale_factor));

  std::string png_data;
  EXPECT_TRUE(app_instance()->GetIconResponse(
      GetAppListIconDimensionForScaleFactor(scale_factor), &png_data));

  std::string icon_data;
  // Read the file from disk and compare with reference data.
  EXPECT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  ASSERT_EQ(icon_data, png_data);
}

TEST_P(ArcAppModelBuilderTest, RemoveAppCleanUpFolder) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const arc::mojom::AppInfo& app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);
  const ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors()[0];

  // No app folder by default.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsIconCreated(prefs, app_id, scale_factor));

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));
  const base::FilePath app_path = prefs->GetAppPath(app_id);

  // Now send generated icon for the app.
  WaitForIconUpdates(profile_.get(), app_id, 1);
  EXPECT_TRUE(IsIconCreated(prefs, app_id, scale_factor));

  // Send empty app list. This will delete app and its folder.
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>());
  // Process pending tasks. This performs multiple thread hops, so we need
  // to run it continuously until it is resolved.
  do {
    content::RunAllTasksUntilIdle();
  } while (IsIconCreated(prefs, app_id, scale_factor));
}

TEST_P(ArcAppModelBuilderTest, LastLaunchTime) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ASSERT_GE(fake_apps().size(), 3U);
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 3));
  const arc::mojom::AppInfo& app1 = fake_apps()[0];
  const arc::mojom::AppInfo& app2 = fake_apps()[1];
  const arc::mojom::AppInfo& app3 = fake_apps()[2];
  const std::string id1 = ArcAppTest::GetAppId(app1);
  const std::string id2 = ArcAppTest::GetAppId(app2);
  const std::string id3 = ArcAppTest::GetAppId(app3);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id1);
  ASSERT_NE(nullptr, app_info.get());

  EXPECT_EQ(base::Time(), app_info->last_launch_time);

  // Test direct setting last launch time.
  const base::Time before_time = base::Time::Now();
  prefs->SetLastLaunchTime(id1);

  app_info = prefs->GetApp(id1);
  ASSERT_NE(nullptr, app_info.get());
  EXPECT_GE(app_info->last_launch_time, before_time);

  // Test setting last launch time via LaunchApp.
  app_info = prefs->GetApp(id2);
  ASSERT_NE(nullptr, app_info.get());
  EXPECT_EQ(base::Time(), app_info->last_launch_time);

  base::Time time_before = base::Time::Now();
  arc::LaunchApp(profile(), id2, ui::EF_NONE,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  const base::Time time_after = base::Time::Now();

  app_info = prefs->GetApp(id2);
  ASSERT_NE(nullptr, app_info.get());
  ASSERT_LE(time_before, app_info->last_launch_time);
  ASSERT_GE(time_after, app_info->last_launch_time);

  // Test last launch time when app is started externally, not from App
  // Launcher.
  app_info = prefs->GetApp(id3);
  ASSERT_NE(nullptr, app_info.get());
  EXPECT_EQ(base::Time(), app_info->last_launch_time);
  time_before = base::Time::Now();
  app_instance()->SendTaskCreated(0, fake_apps()[2], std::string());
  app_info = prefs->GetApp(id3);
  ASSERT_NE(nullptr, app_info.get());
  EXPECT_GE(app_info->last_launch_time, time_before);
}

// Makes sure that install time is set.
TEST_P(ArcAppModelBuilderTest, InstallTime) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  ASSERT_TRUE(fake_apps().size());

  const std::string app_id = ArcAppTest::GetAppId(fake_apps()[0]);
  EXPECT_FALSE(prefs->GetApp(app_id));

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  ASSERT_TRUE(app_info);
  EXPECT_NE(base::Time(), app_info->install_time);
  EXPECT_LE(app_info->install_time, base::Time::Now());
}

TEST_P(ArcAppModelBuilderTest, AppLifeCycleEventsOnOptOut) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  arc::ConnectionObserver<arc::mojom::AppInstance>* const connection_observer =
      prefs;

  arc::MockArcAppListPrefsObserver observer;

  const arc::mojom::AppInfo& app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  ArcAppListPrefs::AppInfo::SetIgnoreCompareInstallTimeForTesting(true);
  const ArcAppListPrefs::AppInfo expected_app_info_registered(
      app.name, app.package_name, app.activity, std::string() /* intent_uri */,
      std::string() /* icon_resource_id */, base::Time() /* last_launch_time */,
      base::Time() /* install_time */, app.sticky, app.notifications_enabled,
      true /* ready */, false /* suspended */, true /* show_in_launcher*/,
      false /* shortcut */, true /* launchable */);

  ArcAppListPrefs::AppInfo expected_app_info_disabled(
      expected_app_info_registered);
  expected_app_info_disabled.ready = false;

  prefs->AddObserver(&observer);

  EXPECT_CALL(observer, OnAppRegistered(app_id, expected_app_info_registered))
      .Times(1);
  EXPECT_CALL(observer, OnAppStatesChanged(app_id, expected_app_info_disabled))
      .Times(1);
  EXPECT_CALL(observer, OnAppRemoved(app_id))
      .Times(arc::ShouldArcAlwaysStart() ? 0 : 1);
  EXPECT_CALL(observer, OnAppIconUpdated(testing::_, testing::_)).Times(0);
  EXPECT_CALL(observer, OnAppNameUpdated(testing::_, testing::_)).Times(0);
  EXPECT_CALL(observer, OnAppLastLaunchTimeUpdated(testing::_)).Times(0);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList({app});

  // On Opt-out ARC app instance is disconnected first and only then
  // notification that ARC is disabled called.
  connection_observer->OnConnectionClosed();
  arc::SetArcPlayStoreEnabledForProfile(profile(), false);

  prefs->RemoveObserver(&observer);
}

// Validate that arc model contains expected elements on restart.
TEST_P(ArcAppModelBuilderRecreate, AppModelRestart) {
  // No apps on initial start.
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());

  // Send info about all fake apps except last.
  std::vector<arc::mojom::AppInfo> apps1(fake_apps().begin(),
                                         fake_apps().end() - 1);
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps1);
  // Model has refreshed apps.
  ValidateHaveApps(apps1);
  EXPECT_EQ(apps1.size(), GetArcItemCount());

  // Simulate restart.
  RestartArc();

  // On restart new model contains last apps.
  ValidateHaveApps(apps1);
  EXPECT_EQ(apps1.size(), GetArcItemCount());

  // Now refresh old apps with new one.
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveApps(fake_apps());
  EXPECT_EQ(fake_apps().size(), GetArcItemCount());
}

TEST_P(ArcPlayStoreAppTest, PlayStore) {
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(arc::kPlayStoreAppId);
  if (GetParam() != ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE) {
    // Make sure PlayStore is available.
    ASSERT_TRUE(app_info);
    EXPECT_FALSE(app_info->ready);
  } else {
    // By default Play Store is not available in case no Play Store mode. But
    // explicitly adding it makes it appear as an ordinal app.
    EXPECT_FALSE(app_info);
  }

  SendPlayStoreApp();

  app_info = prefs->GetApp(arc::kPlayStoreAppId);
  ASSERT_TRUE(app_info);
  EXPECT_TRUE(app_info->ready);

  // TODO(victorhsieh): Opt-out on Persistent ARC is special.  Skip until
  // implemented.
  if (arc::ShouldArcAlwaysStart())
    return;
  arc::SetArcPlayStoreEnabledForProfile(profile(), false);

  app_info = prefs->GetApp(arc::kPlayStoreAppId);
  ASSERT_TRUE(app_info);
  EXPECT_FALSE(app_info->ready);

  arc::LaunchApp(profile(), arc::kPlayStoreAppId, ui::EF_NONE,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
}

TEST_P(ArcPlayStoreAppTest, PaiStarter) {
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  bool pai_started = false;

  arc::ArcPaiStarter starter1(profile_.get(), profile_->GetPrefs());
  arc::ArcPaiStarter starter2(profile_.get(), profile_->GetPrefs());
  EXPECT_FALSE(starter1.started());
  EXPECT_FALSE(starter2.started());
  EXPECT_EQ(app_instance()->start_pai_request_count(), 0);

  starter1.AddOnStartCallback(
      base::BindOnce(&OnPaiStartedCallback, &pai_started));
  EXPECT_FALSE(pai_started);

  arc::ArcSessionManager* session_manager = arc::ArcSessionManager::Get();
  ASSERT_TRUE(session_manager);

  // PAI starter is not expected for ARC without the Play Store.
  if (GetParam() == ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE) {
    EXPECT_FALSE(session_manager->pai_starter());
    return;
  }

  ASSERT_TRUE(session_manager->pai_starter());
  EXPECT_FALSE(session_manager->pai_starter()->started());

  starter2.AcquireLock();

  SendPlayStoreApp();

  EXPECT_TRUE(starter1.started());
  EXPECT_TRUE(pai_started);

  // Test that callback is called immediately in case PAI was already started.
  pai_started = false;
  starter1.AddOnStartCallback(
      base::BindOnce(&OnPaiStartedCallback, &pai_started));
  EXPECT_TRUE(pai_started);

  EXPECT_FALSE(starter2.started());
  EXPECT_TRUE(session_manager->pai_starter()->started());
  EXPECT_EQ(app_instance()->start_pai_request_count(), 2);

  starter2.ReleaseLock();
  EXPECT_TRUE(starter2.started());
  EXPECT_EQ(app_instance()->start_pai_request_count(), 3);

  arc::ArcPaiStarter starter3(profile_.get(), profile_->GetPrefs());
  EXPECT_TRUE(starter3.started());
  EXPECT_EQ(app_instance()->start_pai_request_count(), 4);
}

// Validates that PAI is started on the next session start if it was not started
// during the previous sessions for some reason.
TEST_P(ArcPlayStoreAppTest, StartPaiOnNextRun) {
  if (GetParam() == ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE)
    return;

  arc::ArcSessionManager* session_manager = arc::ArcSessionManager::Get();
  ASSERT_TRUE(session_manager);

  arc::ArcPaiStarter* pai_starter = session_manager->pai_starter();
  ASSERT_TRUE(pai_starter);
  EXPECT_FALSE(pai_starter->started());

  // Finish session with lock. This would prevent running PAI.
  pai_starter->AcquireLock();
  SendPlayStoreApp();
  EXPECT_FALSE(pai_starter->started());
  session_manager->Shutdown();

  // Simulate ARC restart.
  RestartArc();

  // PAI was not started during the previous session due the lock status and
  // should be available now.
  session_manager = arc::ArcSessionManager::Get();
  pai_starter = session_manager->pai_starter();
  ASSERT_TRUE(pai_starter);
  EXPECT_FALSE(pai_starter->locked());

  SendPlayStoreApp();
  EXPECT_TRUE(pai_starter->started());

  // Simulate the next ARC restart.
  RestartArc();

  // PAI was started during the previous session and should not be available
  // now.
  session_manager = arc::ArcSessionManager::Get();
  pai_starter = session_manager->pai_starter();
  EXPECT_FALSE(pai_starter);
}

TEST_P(ArcVoiceInteractionTest, PaiStarterVoiceInteractionNormalFlow) {
  voice_service()->OnAssistantAppRequested();

  SendAssistantAppStarted();
  SendAssistantAppStopped();

  voice_service()->OnVoiceInteractionOobeSetupComplete();

  EXPECT_TRUE(pai_starter()->started());
}

TEST_P(ArcVoiceInteractionTest, PaiStarterVoiceInteractionCancel) {
  voice_service()->OnAssistantCanceled();
  EXPECT_TRUE(pai_starter()->started());
  EXPECT_FALSE(pai_starter()->locked());
}

TEST_P(ArcVoiceInteractionTest, PaiStarterVoiceInteractionAppNotStarted) {
  voice_service()->set_assistant_started_timeout_for_testing(
      base::TimeDelta::FromMilliseconds(100));
  voice_service()->OnAssistantAppRequested();

  WaitForPaiStarted();
}

TEST_P(ArcVoiceInteractionTest, PaiStarterVoiceInteractionWizardNotComplete) {
  voice_service()->set_wizard_completed_timeout_for_testing(
      base::TimeDelta::FromMilliseconds(100));
  voice_service()->OnAssistantAppRequested();

  SendAssistantAppStarted();
  SendAssistantAppStopped();

  WaitForPaiStarted();
}

TEST_P(ArcPlayStoreAppTest,
       FastAppReinstallStarterUserFinishesSelectionBeforePlayStore) {
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  arc::ArcFastAppReinstallStarter starter1(profile_.get(),
                                           profile_->GetPrefs());
  EXPECT_FALSE(starter1.started());
  EXPECT_EQ(0, app_instance()->start_fast_app_reinstall_request_count());

  arc::ArcSessionManager* session_manager = arc::ArcSessionManager::Get();
  ASSERT_TRUE(session_manager);

  // Fast App Reinstall starter is not expected for ARC without the Play Store.
  if (GetParam() == ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE) {
    EXPECT_FALSE(session_manager->fast_app_resintall_starter());
    return;
  }

  ASSERT_TRUE(session_manager->fast_app_resintall_starter());
  EXPECT_FALSE(session_manager->fast_app_resintall_starter()->started());

  // Fast App Reinstall is not expected to start when the user finishes
  // selection without the Play Store.
  base::ListValue package_list;
  package_list.Set(0, std::make_unique<base::Value>("fake_package_name"));
  const base::ListValue* selected_packages(&package_list);
  profile_.get()->GetTestingPrefService()->Set(
      arc::prefs::kArcFastAppReinstallPackages, *selected_packages);
  starter1.OnAppsSelectionFinished();
  EXPECT_FALSE(starter1.started());
  EXPECT_EQ(0, app_instance()->start_fast_app_reinstall_request_count());

  SendPlayStoreApp();

  EXPECT_TRUE(starter1.started());
  EXPECT_EQ(2, app_instance()->start_fast_app_reinstall_request_count());

  arc::ArcFastAppReinstallStarter starter2(profile_.get(),
                                           profile_->GetPrefs());
  EXPECT_TRUE(starter2.started());
  EXPECT_EQ(3, app_instance()->start_fast_app_reinstall_request_count());
}

TEST_P(ArcPlayStoreAppTest,
       FastAppReinstallStarterUserFinishesSelectionAfterPlayStore) {
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  arc::ArcFastAppReinstallStarter starter1(profile_.get(),
                                           profile_->GetPrefs());
  EXPECT_FALSE(starter1.started());
  EXPECT_EQ(0, app_instance()->start_fast_app_reinstall_request_count());

  arc::ArcSessionManager* session_manager = arc::ArcSessionManager::Get();
  ASSERT_TRUE(session_manager);

  // Fast App Reinstall starter is not expected for ARC without the Play Store.
  if (GetParam() == ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE) {
    EXPECT_FALSE(session_manager->fast_app_resintall_starter());
    return;
  }

  ASSERT_TRUE(session_manager->fast_app_resintall_starter());
  EXPECT_FALSE(session_manager->fast_app_resintall_starter()->started());

  SendPlayStoreApp();

  // Fast App Reinstall is not expected to start when the user has not finished
  // selection.
  EXPECT_FALSE(starter1.started());
  EXPECT_EQ(0, app_instance()->start_fast_app_reinstall_request_count());

  base::ListValue package_list;
  package_list.Set(0, std::make_unique<base::Value>("fake_package_name"));
  const base::ListValue* selected_packages(&package_list);
  profile_.get()->GetTestingPrefService()->Set(
      arc::prefs::kArcFastAppReinstallPackages, *selected_packages);
  starter1.OnAppsSelectionFinished();
  // Fast App Reinstall is expected to start right after user finishes selection
  // after Play Store is ready.
  EXPECT_TRUE(starter1.started());
  EXPECT_EQ(1, app_instance()->start_fast_app_reinstall_request_count());

  arc::ArcFastAppReinstallStarter starter2(profile_.get(),
                                           profile_->GetPrefs());
  EXPECT_TRUE(starter2.started());
  EXPECT_EQ(2, app_instance()->start_fast_app_reinstall_request_count());
}

// Test that icon is correctly extracted for shelf group.
TEST_P(ArcAppModelBuilderTest, IconLoaderForShelfGroup) {
  const arc::mojom::AppInfo& app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));
  content::RunAllTasksUntilIdle();

  // Store number of requests generated during the App List item creation. Same
  // request will not be re-sent without clearing the request record in
  // ArcAppListPrefs.
  const size_t initial_icon_request_count =
      app_instance()->icon_requests().size();

  std::vector<arc::mojom::ShortcutInfo> shortcuts =
      arc_test()->fake_shortcuts();
  shortcuts.resize(1);
  shortcuts[0].intent_uri +=
      ";S.org.chromium.arc.shelf_group_id=arc_test_shelf_group;end";
  app_instance()->SendInstallShortcuts(shortcuts);
  const std::string shortcut_id = ArcAppTest::GetAppId(shortcuts[0]);
  content::RunAllTasksUntilIdle();

  const std::string id_shortcut_exist =
      arc::ArcAppShelfId("arc_test_shelf_group", app_id).ToString();
  const std::string id_shortcut_absent =
      arc::ArcAppShelfId("arc_test_shelf_group_absent", app_id).ToString();

  FakeAppIconLoaderDelegate delegate;
  ArcAppIconLoader icon_loader(
      profile(), app_list::AppListConfig::instance().grid_icon_dimension(),
      &delegate);
  EXPECT_EQ(0UL, delegate.update_image_count());

  // Fetch original app icon.
  icon_loader.FetchImage(app_id);
  delegate.WaitForIconUpdates(ui::GetSupportedScaleFactors().size());
  EXPECT_EQ(app_id, delegate.app_id());
  const gfx::ImageSkia app_icon = delegate.image();

  // Shortcut exists, icon is requested from shortcut.
  icon_loader.FetchImage(id_shortcut_exist);
  // Icon was sent on request and loader should be updated.
  delegate.WaitForIconUpdates(ui::GetSupportedScaleFactors().size());
  EXPECT_EQ(id_shortcut_exist, delegate.app_id());

  // Validate that fetched shortcut icon for existing shortcut does not match
  // referenced app icon.
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(1.0f).GetBitmap(),
      delegate.image().GetRepresentation(1.0f).GetBitmap()));

  content::RunAllTasksUntilIdle();
  const size_t shortcut_request_count =
      app_instance()->shortcut_icon_requests().size();
  EXPECT_NE(0U, shortcut_request_count);
  EXPECT_EQ(initial_icon_request_count, app_instance()->icon_requests().size());
  for (const auto& request : app_instance()->shortcut_icon_requests())
    EXPECT_EQ(shortcuts[0].icon_resource_id, request->icon_resource_id());

  // Fallback when shortcut is not found for shelf group id, use app id instead.
  // Remove the IconRequestRecord for |app_id| to observe the icon request for
  // |app_id| is re-sent.
  const size_t update_image_count_before = delegate.update_image_count();
  MaybeRemoveIconRequestRecord(app_id);
  icon_loader.FetchImage(id_shortcut_absent);
  // Expected default update.
  EXPECT_EQ(update_image_count_before + 1, delegate.update_image_count());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(shortcut_request_count,
            app_instance()->shortcut_icon_requests().size());

  // Validate that fetched shortcut icon for absent shortcut contains referenced
  // app icon.
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(1.0f).GetBitmap(),
      delegate.image().GetRepresentation(1.0f).GetBitmap()));
}

// Test that icon is correctly updated for suspended/non-suspended app.
TEST_P(ArcAppModelBuilderTest, IconLoaderForSuspendedApps) {
  arc::mojom::AppInfo app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  FakeAppIconLoaderDelegate delegate;
  ArcAppIconLoader icon_loader(
      profile(), app_list::AppListConfig::instance().grid_icon_dimension(),
      &delegate);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList({app});
  content::RunAllTasksUntilIdle();

  icon_loader.FetchImage(app_id);
  delegate.WaitForIconUpdates(2);

  const gfx::ImageSkia app_normal_icon = delegate.image();

  const size_t update_count = delegate.update_image_count();
  // Now switch to suspended mode. Image is updated inline because primary icon
  // is loaded and we only apply gray effect.
  app.suspended = true;
  app_instance()->SendPackageAppListRefreshed(app.package_name, {app});
  EXPECT_EQ(update_count + 1, delegate.update_image_count());
  // No futher updates.
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(update_count + 1, delegate.update_image_count());

  // We should have different icons.
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(
      app_normal_icon.GetRepresentation(1.0f).GetBitmap(),
      delegate.image().GetRepresentation(1.0f).GetBitmap()));

  // Now switch back to normal mode.
  app.suspended = false;
  app_instance()->SendPackageAppListRefreshed(app.package_name, {app});
  EXPECT_EQ(update_count + 2, delegate.update_image_count());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(update_count + 2, delegate.update_image_count());

  // Icon should be restored to normal
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      app_normal_icon.GetRepresentation(1.0f).GetBitmap(),
      delegate.image().GetRepresentation(1.0f).GetBitmap()));
}

// If the cached icon file is corrupted, we expect send request to ARC for a new
// icon.
TEST_P(ArcAppModelBuilderTest, IconLoaderWithBadIcon) {
  const arc::mojom::AppInfo& app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->set_icon_response_type(
      arc::FakeAppInstance::IconResponseType::ICON_RESPONSE_SEND_BAD);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));
  content::RunAllTasksUntilIdle();

  // Store number of requests generated during the App List item creation. Same
  // request will not be re-sent without clearing the request record in
  // ArcAppListPrefs.
  const size_t initial_icon_request_count =
      app_instance()->icon_requests().size();

  FakeAppIconLoaderDelegate delegate;
  ArcAppIconLoader icon_loader(
      profile(), app_list::AppListConfig::instance().grid_icon_dimension(),
      &delegate);
  icon_loader.FetchImage(app_id);

  // So far one updated of default icon is expected.
  EXPECT_EQ(delegate.update_image_count(), 1U);

  // Although icon file is still missing, expect no new request sent to ARC as
  // them are recorded in IconRequestRecord in ArcAppListPrefs.
  EXPECT_EQ(app_instance()->icon_requests().size(), initial_icon_request_count);
  // Validate default image.
  ValidateIcon(delegate.image());

  MaybeRemoveIconRequestRecord(app_id);

  // Install Bad image.
  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  ArcAppItem* app_item = FindArcItem(app_id);
  for (auto& scale_factor : scale_factors) {
    // Force the icon to be loaded.
    app_item->icon().GetRepresentation(
        ui::GetScaleForScaleFactor(scale_factor));
    WaitForIconCreation(prefs, app_id, scale_factor);
  }

  // After clear request record related to |app_id|, when bad icon is installed,
  // decoding failure will trigger re-sending new icon request to ARC.
  EXPECT_TRUE(app_instance()->icon_requests().size() >
              initial_icon_request_count);
  for (size_t i = initial_icon_request_count;
       i < app_instance()->icon_requests().size(); ++i) {
    const auto& request = app_instance()->icon_requests()[i];
    EXPECT_TRUE(request->IsForApp(app));
  }

  // Icon update is not expected because of bad icon.
  EXPECT_EQ(delegate.update_image_count(), 1U);
}

TEST_P(ArcAppModelBuilderTest, IconLoader) {
  const arc::mojom::AppInfo& app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));

  FakeAppIconLoaderDelegate delegate;
  ArcAppIconLoader icon_loader(
      profile(), app_list::AppListConfig::instance().grid_icon_dimension(),
      &delegate);
  EXPECT_EQ(0UL, delegate.update_image_count());
  icon_loader.FetchImage(app_id);
  EXPECT_EQ(1UL, delegate.update_image_count());
  EXPECT_EQ(app_id, delegate.app_id());

  // Validate default image.
  ValidateIcon(delegate.image());

  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  ArcAppItem* app_item = FindArcItem(app_id);
  for (auto& scale_factor : scale_factors) {
    // Force the icon to be loaded.
    app_item->icon().GetRepresentation(
        ui::GetScaleForScaleFactor(scale_factor));
  }

  delegate.WaitForIconUpdates(scale_factors.size());

  // Validate loaded image.
  EXPECT_EQ(1 + scale_factors.size(), delegate.update_image_count());
  EXPECT_EQ(app_id, delegate.app_id());
  ValidateIcon(delegate.image());

  // No more updates are expected.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1 + scale_factors.size(), delegate.update_image_count());
}

TEST_P(ArcAppModelBuilderRecreate, IconInvalidation) {
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported_scale_factors(
      supported_scale_factors);

  arc::mojom::ArcPackageInfo package;
  package.package_name = fake_apps()[0].package_name;
  package.package_version = 1;
  package.last_backup_android_id = 1;
  package.last_backup_time = 1;
  package.sync = true;

  ASSERT_FALSE(fake_apps().empty());
  std::vector<arc::mojom::AppInfo> apps = std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1);

  const arc::mojom::AppInfo& app = apps[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps);
  AddPackage(package);

  prefs->MaybeRequestIcon(app_id,
                          GetAppListIconDescriptor(ui::SCALE_FACTOR_100P));

  WaitForIconUpdates(profile_.get(), app_id, 1);

  // Simulate ARC restart.
  RestartArc();

  prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps);
  app_instance()->SendPackageModified(package);

  // No icon update requests on restart. Icons were not invalidated.
  EXPECT_TRUE(app_instance()->icon_requests().empty());

  // Send new apps for the package. This should invalidate package icons.
  package.package_version = 2;
  app_instance()->SendPackageAppListRefreshed(apps[0].package_name, apps);
  app_instance()->SendPackageModified(package);
  base::RunLoop().RunUntilIdle();

  // Requests to reload icons are issued for all supported scales.
  const std::vector<std::unique_ptr<arc::FakeAppInstance::IconRequest>>&
      icon_requests = app_instance()->icon_requests();
  ASSERT_EQ(2U, icon_requests.size());
  EXPECT_TRUE(icon_requests[0]->IsForApp(app));
  EXPECT_EQ(GetAppListIconDimensionForScaleFactor(ui::SCALE_FACTOR_100P),
            icon_requests[0]->dimension());
  EXPECT_TRUE(icon_requests[1]->IsForApp(app));
  EXPECT_EQ(GetAppListIconDimensionForScaleFactor(ui::SCALE_FACTOR_200P),
            icon_requests[1]->dimension());

  WaitForIconUpdates(profile_.get(), app_id, 2);

  // Simulate ARC restart again.
  RestartArc();

  prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps);
  app_instance()->SendPackageModified(package);

  // No new icon update requests on restart. Icons were invalidated and updated.
  EXPECT_TRUE(app_instance()->icon_requests().empty());
}

TEST_P(ArcAppModelBuilderTest, IconLoadNonSupportedScales) {
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported_scale_factors(
      supported_scale_factors);

  // Initialize one ARC app.
  const arc::mojom::AppInfo& app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));

  FakeAppIconLoaderDelegate delegate;
  ArcAppIconLoader icon_loader(
      profile(), app_list::AppListConfig::instance().grid_icon_dimension(),
      &delegate);
  icon_loader.FetchImage(app_id);
  // Expected 1 update with default image and 2 representations should be
  // allocated.
  EXPECT_EQ(1U, delegate.update_image_count());
  gfx::ImageSkia app_icon = delegate.image();
  EXPECT_EQ(2U, app_icon.image_reps().size());
  EXPECT_TRUE(app_icon.HasRepresentation(1.0f));
  EXPECT_TRUE(app_icon.HasRepresentation(2.0f));

  // Request non-supported scales. Cached supported representations with
  // default image should be used. 1.0 is used to scale 1.15 and
  // 2.0 is used to scale 1.25.
  app_icon.GetRepresentation(1.15f);
  app_icon.GetRepresentation(1.25f);
  EXPECT_EQ(1U, delegate.update_image_count());
  EXPECT_EQ(4U, app_icon.image_reps().size());
  EXPECT_TRUE(app_icon.HasRepresentation(1.0f));
  EXPECT_TRUE(app_icon.HasRepresentation(2.0f));
  EXPECT_TRUE(app_icon.HasRepresentation(1.15f));
  EXPECT_TRUE(app_icon.HasRepresentation(1.25f));

  // Keep default images for reference.
  const SkBitmap bitmap_1_0 = app_icon.GetRepresentation(1.0f).GetBitmap();
  const SkBitmap bitmap_1_15 = app_icon.GetRepresentation(1.15f).GetBitmap();
  const SkBitmap bitmap_1_25 = app_icon.GetRepresentation(1.25f).GetBitmap();
  const SkBitmap bitmap_2_0 = app_icon.GetRepresentation(2.0f).GetBitmap();

  delegate.WaitForIconUpdates(1);

  EXPECT_FALSE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(1.0f).GetBitmap(), bitmap_1_0));
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(1.15f).GetBitmap(), bitmap_1_15));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(1.25f).GetBitmap(), bitmap_1_25));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(2.0f).GetBitmap(), bitmap_2_0));

  // Send icon image for 200P. 2.0 and 1.25 should be updated.
  delegate.WaitForIconUpdates(1);

  EXPECT_FALSE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(1.0f).GetBitmap(), bitmap_1_0));
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(1.15f).GetBitmap(), bitmap_1_15));
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(1.25f).GetBitmap(), bitmap_1_25));
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(
      app_icon.GetRepresentation(2.0f).GetBitmap(), bitmap_2_0));
}

TEST_P(ArcAppModelBuilderTest, AppLauncher) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
  ASSERT_NE(nullptr, prefs);

  // App1 is called in deferred mode, after refreshing apps.
  // App2 is never called since app is not avaialble.
  // App3 is never called immediately because app is available already.
  const arc::mojom::AppInfo& app1 = fake_apps()[0];
  const arc::mojom::AppInfo& app2 = fake_apps()[1];
  const arc::mojom::AppInfo& app3 = fake_apps()[2];
  const std::string id1 = ArcAppTest::GetAppId(app1);
  const std::string id2 = ArcAppTest::GetAppId(app2);
  const std::string id3 = ArcAppTest::GetAppId(app3);

  ArcAppLauncher launcher1(profile(), id1, base::Optional<std::string>(), false,
                           display::kInvalidDisplayId,
                           arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_FALSE(launcher1.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher1));

  ArcAppLauncher launcher3(profile(), id3, base::Optional<std::string>(), false,
                           display::kInvalidDisplayId,
                           arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_FALSE(launcher1.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher1));
  EXPECT_FALSE(launcher3.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher3));

  EXPECT_EQ(0u, app_instance()->launch_requests().size());

  std::vector<arc::mojom::AppInfo> apps(fake_apps().begin(),
                                        fake_apps().begin() + 2);
  app_instance()->SendRefreshAppList(apps);

  EXPECT_TRUE(launcher1.app_launched());
  ASSERT_EQ(1u, app_instance()->launch_requests().size());
  EXPECT_TRUE(app_instance()->launch_requests()[0]->IsForApp(app1));
  EXPECT_FALSE(launcher3.app_launched());
  EXPECT_FALSE(prefs->HasObserver(&launcher1));
  EXPECT_TRUE(prefs->HasObserver(&launcher3));

  const std::string launch_intent2 = arc::GetLaunchIntent(
      app2.package_name, app2.activity, std::vector<std::string>());
  ArcAppLauncher launcher2(profile(), id2, launch_intent2, false,
                           display::kInvalidDisplayId,
                           arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_TRUE(launcher2.app_launched());
  EXPECT_FALSE(prefs->HasObserver(&launcher2));
  EXPECT_EQ(1u, app_instance()->launch_requests().size());
  ASSERT_EQ(1u, app_instance()->launch_intents().size());
  EXPECT_EQ(app_instance()->launch_intents()[0], launch_intent2);
}

// Suspended app cannot be triggered from app launcher.
TEST_P(ArcAppModelBuilderTest, AppLauncherForSuspendedApp) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
  ASSERT_NE(nullptr, prefs);

  arc::mojom::AppInfo app = fake_apps()[0];
  app.suspended = true;
  const std::string app_id = ArcAppTest::GetAppId(app);

  ArcAppLauncher launcher(profile(), app_id, base::Optional<std::string>(),
                          false, display::kInvalidDisplayId,
                          arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_FALSE(launcher.app_launched());

  // Register app, however it is suspended.
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList({app});
  EXPECT_FALSE(launcher.app_launched());
  EXPECT_TRUE(app_instance()->launch_requests().empty());

  // Update app with non-suspended state.
  app.suspended = false;
  app_instance()->SendPackageAppListRefreshed(app.package_name, {app});
  EXPECT_TRUE(launcher.app_launched());

  ASSERT_EQ(1u, app_instance()->launch_requests().size());
  EXPECT_TRUE(app_instance()->launch_requests()[0]->IsForApp(app));
}

// Validates an app that have no launchable flag.
TEST_P(ArcAppModelBuilderTest, NonLaunchableApp) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  app_instance()->RefreshAppList();
  // Send all except first.
  std::vector<arc::mojom::AppInfo> apps(fake_apps().begin() + 1,
                                        fake_apps().end());
  app_instance()->SendRefreshAppList(apps);
  ValidateHaveApps(apps);

  const std::string app_id = ArcAppTest::GetAppId(fake_apps()[0]);

  EXPECT_FALSE(prefs->IsRegistered(app_id));
  EXPECT_FALSE(FindArcItem(app_id));
  app_instance()->SendTaskCreated(0, fake_apps()[0], std::string());
  // App should not appear now in the model but should be registered.
  EXPECT_FALSE(FindArcItem(app_id));
  EXPECT_TRUE(prefs->IsRegistered(app_id));
}

TEST_P(ArcAppModelBuilderTest, ArcAppsAndShortcutsOnPackageChange) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  std::vector<arc::mojom::AppInfo> apps = fake_apps();
  ASSERT_GE(apps.size(), 3U);
  apps[0].package_name = apps[2].package_name;
  apps[1].package_name = apps[2].package_name;

  std::vector<arc::mojom::ShortcutInfo> shortcuts = fake_shortcuts();
  for (auto& shortcut : shortcuts)
    shortcut.package_name = apps[0].package_name;

  // Second app should be preserved after update.
  std::vector<arc::mojom::AppInfo> apps1(apps.begin(), apps.begin() + 2);
  std::vector<arc::mojom::AppInfo> apps2(apps.begin() + 1, apps.begin() + 3);

  // Adding package is required to safely call SendPackageUninstalled.
  arc::mojom::ArcPackageInfo package;
  package.package_name = apps[0].package_name;
  package.package_version = 1;
  package.sync = true;
  AddPackage(package);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps1);
  app_instance()->SendInstallShortcuts(shortcuts);

  ValidateHaveAppsAndShortcuts(apps1, shortcuts);

  const std::string app_id = ArcAppTest::GetAppId(apps[1]);
  const base::Time time_before = base::Time::Now();
  prefs->SetLastLaunchTime(app_id);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info_before =
      prefs->GetApp(app_id);
  ASSERT_TRUE(app_info_before);
  EXPECT_GE(base::Time::Now(), time_before);

  app_instance()->SendPackageAppListRefreshed(apps[0].package_name, apps2);
  ValidateHaveAppsAndShortcuts(apps2, shortcuts);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info_after =
      prefs->GetApp(app_id);
  ASSERT_TRUE(app_info_after);
  EXPECT_EQ(app_info_before->last_launch_time,
            app_info_after->last_launch_time);

  RemovePackage(package);
  ValidateHaveAppsAndShortcuts(std::vector<arc::mojom::AppInfo>(),
                               std::vector<arc::mojom::ShortcutInfo>());
}

TEST_P(ArcDefaulAppTest, DefaultApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  ValidateHaveApps(fake_default_apps());

  // Start normal apps. We should have apps from 2 subsets.
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());

  std::vector<arc::mojom::AppInfo> all_apps = fake_default_apps();
  all_apps.insert(all_apps.end(), fake_apps().begin(), fake_apps().end());
  ValidateHaveApps(all_apps);

  // However default apps are still not ready.
  for (const auto& default_app : fake_default_apps()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        prefs->GetApp(ArcAppTest::GetAppId(default_app));
    ASSERT_TRUE(app_info);
    EXPECT_FALSE(app_info->ready);
    EXPECT_NE(base::Time(), app_info->install_time);
  }

  // Install default apps.
  for (const auto& default_app : fake_default_apps()) {
    std::vector<arc::mojom::AppInfo> package_apps;
    package_apps.push_back(default_app);
    app_instance()->SendPackageAppListRefreshed(default_app.package_name,
                                                package_apps);

    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        prefs->GetApp(ArcAppTest::GetAppId(default_app));
    ASSERT_TRUE(app_info);
    EXPECT_NE(base::Time(), app_info->install_time);
  }

  // And now default apps are ready.
  std::map<std::string, bool> oem_states;
  for (const auto& default_app : fake_default_apps()) {
    const std::string app_id = ArcAppTest::GetAppId(default_app);
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    ASSERT_TRUE(app_info);
    EXPECT_TRUE(app_info->ready);
    oem_states[app_id] = prefs->IsOem(app_id);
  }

  // Uninstall first default package. Default app should go away.
  app_instance()->SendPackageUninstalled(all_apps[0].package_name);
  all_apps.erase(all_apps.begin());
  ValidateHaveApps(all_apps);

  // OptOut and default apps should exist minus first.
  // TODO(victorhsieh): Opt-out on Persistent ARC is special.  Skip until
  // implemented.
  if (arc::ShouldArcAlwaysStart())
    return;
  arc::SetArcPlayStoreEnabledForProfile(profile(), false);
  all_apps = fake_default_apps();
  all_apps.erase(all_apps.begin());
  ValidateHaveApps(all_apps);

  // Sign-out and sign-in again. Removed default app should not appear.
  RestartArc();

  // Prefs are changed.
  prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);
  ValidateHaveApps(all_apps);

  // Install deleted default app again.
  std::vector<arc::mojom::AppInfo> package_apps;
  package_apps.push_back(fake_default_apps()[0]);
  app_instance()->SendPackageAppListRefreshed(
      fake_default_apps()[0].package_name, package_apps);
  ValidateHaveApps(fake_default_apps());

  // Validate that OEM state is preserved.
  for (const auto& default_app : fake_default_apps()) {
    const std::string app_id = ArcAppTest::GetAppId(default_app);
    EXPECT_TRUE(prefs->IsDefault(app_id));
    EXPECT_EQ(oem_states[app_id], prefs->IsOem(app_id));
  }
}

// Test that validates disabling default app removes app from the list and this
// is persistent in next sessions.
TEST_P(ArcDefaulAppTest, DisableDefaultApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  ValidateHaveApps(fake_default_apps());

  // Install default app.
  const arc::mojom::AppInfo default_app = fake_default_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(default_app);
  std::vector<arc::mojom::AppInfo> package_apps;
  package_apps.push_back(default_app);
  app_instance()->SendPackageAppListRefreshed(default_app.package_name,
                                              package_apps);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  ASSERT_TRUE(app_info);
  EXPECT_TRUE(app_info->ready);
  EXPECT_TRUE(prefs->IsDefault(app_id));

  // Disable default app. In this case list of apps for package is empty.
  package_apps.clear();
  app_instance()->SendPackageAppListRefreshed(default_app.package_name,
                                              package_apps);
  EXPECT_FALSE(prefs->GetApp(app_id));

  // Sign-out and sign-in again. Disabled default app should not appear.
  RestartArc();

  prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);
  EXPECT_FALSE(prefs->GetApp(app_id));
}

TEST_P(ArcAppLauncherForDefaulAppTest, AppIconUpdated) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  ASSERT_FALSE(fake_default_apps().empty());
  const arc::mojom::AppInfo& app = fake_default_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  EXPECT_FALSE(prefs->GetApp(app_id));
  EXPECT_TRUE(prefs
                  ->MaybeGetIconPathForDefaultApp(
                      app_id, GetAppListIconDescriptor(ui::SCALE_FACTOR_100P))
                  .empty());
  arc_test()->WaitForDefaultApps();
  EXPECT_TRUE(prefs->GetApp(app_id));
  EXPECT_FALSE(prefs
                   ->MaybeGetIconPathForDefaultApp(
                       app_id, GetAppListIconDescriptor(ui::SCALE_FACTOR_100P))
                   .empty());

  // Icon can be only fetched after app is registered in the system.
  FakeAppIconLoaderDelegate icon_delegate;
  std::unique_ptr<ArcAppIconLoader> icon_loader =
      std::make_unique<ArcAppIconLoader>(
          profile(), app_list::AppListConfig::instance().grid_icon_dimension(),
          &icon_delegate);
  icon_loader->FetchImage(app_id);
  icon_delegate.WaitForIconUpdates(ui::GetSupportedScaleFactors().size());
  icon_loader.reset();

  // Restart ARC to validate default app icon can be loaded next session.
  RestartArc();
  prefs = ArcAppListPrefs::Get(profile_.get());

  FakeAppIconLoaderDelegate icon_delegate2;
  icon_loader = std::make_unique<ArcAppIconLoader>(
      profile(), app_list::AppListConfig::instance().grid_icon_dimension(),
      &icon_delegate2);
  icon_loader->FetchImage(app_id);
  // Default app icon becomes available once default apps loaded
  // (asynchronously).
  EXPECT_TRUE(prefs
                  ->MaybeGetIconPathForDefaultApp(
                      app_id, GetAppListIconDescriptor(ui::SCALE_FACTOR_100P))
                  .empty());
  icon_delegate2.WaitForIconUpdates(ui::GetSupportedScaleFactors().size());
  EXPECT_FALSE(prefs
                   ->MaybeGetIconPathForDefaultApp(
                       app_id, GetAppListIconDescriptor(ui::SCALE_FACTOR_100P))
                   .empty());
  icon_loader.reset();
}

// Validates that default app icon can be loaded for non-default dips, that do
// not exist in Chrome image.
TEST_P(ArcAppLauncherForDefaulAppTest, AppIconNonDefaultDip) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  ASSERT_FALSE(fake_default_apps().empty());
  const arc::mojom::AppInfo& app = fake_default_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  // Icon can be only fetched after app is registered in the system.
  arc_test()->WaitForDefaultApps();

  FakeAppIconLoaderDelegate icon_delegate;
  // 17 should never be a default dip size.
  std::unique_ptr<ArcAppIconLoader> icon_loader =
      std::make_unique<ArcAppIconLoader>(profile(), 17, &icon_delegate);
  icon_loader->FetchImage(app_id);
  icon_delegate.WaitForIconUpdates(ui::GetSupportedScaleFactors().size());
  icon_loader.reset();
}

TEST_P(ArcAppLauncherForDefaulAppTest, AppLauncherForDefaultApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  ASSERT_GE(fake_default_apps().size(), 2U);
  const arc::mojom::AppInfo& app1 = fake_default_apps()[0];
  const arc::mojom::AppInfo& app2 = fake_default_apps()[1];
  const std::string id1 = ArcAppTest::GetAppId(app1);
  const std::string id2 = ArcAppTest::GetAppId(app2);

  // Launch when app is registered and ready.
  ArcAppLauncher launcher1(profile(), id1, base::Optional<std::string>(), false,
                           display::kInvalidDisplayId,
                           arc::UserInteractionType::NOT_USER_INITIATED);
  // Launch when app is registered.
  ArcAppLauncher launcher2(profile(), id2, base::Optional<std::string>(), true,
                           display::kInvalidDisplayId,
                           arc::UserInteractionType::NOT_USER_INITIATED);

  EXPECT_FALSE(launcher1.app_launched());

  arc_test()->WaitForDefaultApps();

  // Only second app is expected to be launched.
  EXPECT_FALSE(launcher1.app_launched());
  EXPECT_TRUE(launcher2.app_launched());

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_default_apps());
  // Default apps are ready now and it is expected that first app was launched
  // now.
  EXPECT_TRUE(launcher1.app_launched());
}

TEST_P(ArcDefaulAppTest, DefaultAppsNotAvailable) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  ValidateHaveApps(fake_default_apps());

  const std::vector<arc::mojom::AppInfo> empty_app_list;

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(empty_app_list);

  std::vector<arc::mojom::AppInfo> expected_apps(fake_default_apps());
  ValidateHaveApps(expected_apps);

  if (GetParam() == ArcState::ARC_PERSISTENT_WITHOUT_PLAY_STORE) {
    prefs->SimulateDefaultAppAvailabilityTimeoutForTesting();
    ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
    return;
  }

  // PAI was not started and we should not have any active timer for default
  // apps.
  prefs->SimulateDefaultAppAvailabilityTimeoutForTesting();
  ValidateHaveApps(expected_apps);

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  ASSERT_TRUE(arc_session_manager);

  arc::ArcPaiStarter* pai_starter = arc_session_manager->pai_starter();
  ASSERT_TRUE(pai_starter);

  EXPECT_FALSE(pai_starter->started());

  // Play store app triggers PAI.
  arc::mojom::AppInfo app;
  app.name = "Play Store";
  app.package_name = arc::kPlayStorePackage;
  app.activity = arc::kPlayStoreActivity;
  app_instance()->RefreshAppList();

  std::vector<arc::mojom::AppInfo> only_play_store({app});
  app_instance()->SendRefreshAppList(only_play_store);
  expected_apps.push_back(app);

  // Timer was set to detect not available default apps.
  ValidateHaveApps(expected_apps);

  prefs->SimulateDefaultAppAvailabilityTimeoutForTesting();

  // No default app installation and already installed packages.
  ValidateHaveApps(only_play_store);
}

TEST_P(ArcDefaulAppTest, DefaultAppsInstallation) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const std::vector<arc::mojom::AppInfo> empty_app_list;

  ValidateHaveApps(fake_default_apps());

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(empty_app_list);

  ValidateHaveApps(fake_default_apps());

  // Notify that default installations have been started.
  for (const auto& fake_app : fake_default_apps())
    app_instance()->SendInstallationStarted(fake_app.package_name);

  // Timeout does not affect default app availability because all installations
  // for default apps have been started.
  prefs->SimulateDefaultAppAvailabilityTimeoutForTesting();
  ValidateHaveApps(fake_default_apps());

  const arc::mojom::AppInfo& app_last = fake_default_apps().back();
  std::vector<arc::mojom::AppInfo> available_apps = fake_default_apps();
  available_apps.pop_back();

  for (const auto& fake_app : available_apps)
    app_instance()->SendInstallationFinished(fake_app.package_name, true);

  // So far we have all default apps available because not all installations
  // completed.
  ValidateHaveApps(fake_default_apps());

  // Last default app installation failed.
  app_instance()->SendInstallationFinished(app_last.package_name, false);

  // We should have all default apps except last.
  ValidateHaveApps(available_apps);
}

TEST_P(ArcDefaulAppForManagedUserTest, DefaultAppsForManagedUser) {
  const ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  // There is no default app for managed users except Play Store
  for (const auto& app : fake_default_apps()) {
    const std::string app_id = ArcAppTest::GetAppId(app);
    EXPECT_FALSE(prefs->IsRegistered(app_id));
    EXPECT_FALSE(prefs->GetApp(app_id));
  }

  // PlayStor exists for managed and enabled state.
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(arc::kPlayStoreAppId);
  if (IsEnabledByPolicy() &&
      GetParam() !=
          ArcState::ARC_PERSISTENT_MANAGED_ENABLED_AND_PLAY_STORE_HIDDEN) {
    ASSERT_TRUE(app_info);
    EXPECT_FALSE(app_info->ready);
  } else {
    EXPECT_FALSE(prefs->IsRegistered(arc::kPlayStoreAppId));
    EXPECT_FALSE(app_info);
  }
}

INSTANTIATE_TEST_CASE_P(,
                        ArcAppModelBuilderTest,
                        ::testing::ValuesIn(kUnmanagedArcStates));
INSTANTIATE_TEST_CASE_P(,
                        ArcDefaulAppTest,
                        ::testing::ValuesIn(kUnmanagedArcStates));
INSTANTIATE_TEST_CASE_P(,
                        ArcAppLauncherForDefaulAppTest,
                        ::testing::ValuesIn(kUnmanagedArcStates));
INSTANTIATE_TEST_CASE_P(,
                        ArcDefaulAppForManagedUserTest,
                        ::testing::ValuesIn(kManagedArcStates));
INSTANTIATE_TEST_CASE_P(,
                        ArcPlayStoreAppTest,
                        ::testing::ValuesIn(kUnmanagedArcStates));
INSTANTIATE_TEST_CASE_P(,
                        ArcVoiceInteractionTest,
                        ::testing::ValuesIn(kUnmanagedArcStatesWithPlayStore));
INSTANTIATE_TEST_CASE_P(,
                        ArcAppModelBuilderRecreate,
                        ::testing::ValuesIn(kUnmanagedArcStates));
