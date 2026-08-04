// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/app_constants/constants.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/experiences/arc/app/arc_app_constants.h"
#include "components/user_manager/user_manager.h"

namespace {

std::vector<arc::mojom::AppInfoPtr> GetArcSettingsAppInfo() {
  std::vector<arc::mojom::AppInfoPtr> apps;
  arc::mojom::AppInfoPtr app(arc::mojom::AppInfo::New());
  app->name = "settings";
  app->package_name = "com.android.settings";
  app->activity = "com.android.settings.Settings";
  app->sticky = false;
  apps.push_back(std::move(app));
  return apps;
}

}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestOtherExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

}  // namespace

class AppInfoDialogViewsTest : public ChromeViewsTestBase,
                               public views::WidgetObserver {
 public:
  AppInfoDialogViewsTest() = default;

  AppInfoDialogViewsTest(const AppInfoDialogViewsTest&) = delete;
  AppInfoDialogViewsTest& operator=(const AppInfoDialogViewsTest&) = delete;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    arc_app_test_ =
        std::make_unique<ArcAppTest>(ArcAppTest::UserManagerMode::kCreate);
    arc_app_test_->PreProfileSetUp();
#endif

    ChromeViewsTestBase::SetUp();

#if BUILDFLAG(IS_CHROMEOS)
    arc_app_test_->PostProfileSetUp(extension_environment_.profile());

    shelf_model_ = std::make_unique<ash::ShelfModel>();
    browser_controller_.emplace();
    chrome_shelf_controller_ = std::make_unique<ChromeShelfController>(
        extension_environment_.profile(), shelf_model_.get());
    chrome_shelf_controller_->SetProfileForTest(
        extension_environment_.profile());
    chrome_shelf_controller_->SetShelfControllerHelperForTest(
        std::make_unique<ShelfControllerHelper>(
            extension_environment_.profile()));
    chrome_shelf_controller_->Init();
#endif
    extension_ = extension_environment_.MakePackagedApp(kTestExtensionId, true);
    chrome_app_ = extension_environment_.MakePackagedApp(
        app_constants::kChromeAppId, true);
  }

  void TearDown() override {
    CloseAppInfo();
    extension_ = nullptr;
    chrome_app_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS)
    chrome_shelf_controller_.reset();
    browser_controller_.reset();
    shelf_model_.reset();
    CHECK(arc_app_test_);
    arc_app_test_->PreProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)

    extension_environment_.DeleteProfile();

    ChromeViewsTestBase::TearDown();

#if BUILDFLAG(IS_CHROMEOS)
    arc_app_test_->PostProfileTearDown();
    arc_app_test_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

 protected:
  void ShowAppInfo(const std::string& app_id) {
    ShowAppInfoForProfile(app_id, extension_environment_.profile());
  }

  void ShowAppInfoForProfile(const std::string& app_id, Profile* profile) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)
            ->enabled_extensions()
            .GetByID(app_id);
    DCHECK(extension);

    DCHECK(!widget_);
    widget_ = views::DialogDelegate::CreateDialogWidget(
        new views::DialogDelegateView(), GetContext(), gfx::NativeView());
    widget_->AddObserver(this);
    dialog_ = widget_->GetContentsView()->AddChildView(
        std::make_unique<AppInfoDialog>(profile, extension));
    widget_->Show();
  }

  void CloseAppInfo() {
    if (widget_) {
      widget_->CloseNow();
    }
    base::RunLoop().RunUntilIdle();
    DCHECK(!widget_);
  }

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }

  void UninstallApp(const std::string& app_id) {
    extensions::ExtensionRegistrar::Get(extension_environment_.profile())
        ->UninstallExtension(
            app_id, extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING,
            nullptr);
  }

  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<AppInfoDialog, AcrossTasksDanglingUntriaged> dialog_ =
      nullptr;  // Owned by |widget_|'s views hierarchy.
  scoped_refptr<const extensions::Extension> extension_;
  scoped_refptr<const extensions::Extension> chrome_app_;
  extensions::TestExtensionEnvironment extension_environment_{
      extensions::TestExtensionEnvironment::Type::
          kInheritExistingTaskEnvironment,
      extensions::TestExtensionEnvironment::ProfileCreationType::kCreate,
#if BUILDFLAG(IS_CHROMEOS)
      extensions::TestExtensionEnvironment::OSSetupType::kNoSetUp,
#endif
  };
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::ShelfModel> shelf_model_;
  std::optional<ash::BrowserControllerImpl> browser_controller_;
  std::unique_ptr<ChromeShelfController> chrome_shelf_controller_;
  std::unique_ptr<ArcAppTest> arc_app_test_;
#endif
};

// Tests that the dialog closes when the current app is uninstalled.
TEST_F(AppInfoDialogViewsTest, UninstallingAppClosesDialog) {
  ShowAppInfo(kTestExtensionId);
  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  UninstallApp(kTestExtensionId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget_);
}

// Tests that the dialog does not close when a different app is uninstalled.
TEST_F(AppInfoDialogViewsTest, UninstallingOtherAppDoesNotCloseDialog) {
  ShowAppInfo(kTestExtensionId);
  extension_environment_.MakePackagedApp(kTestOtherExtensionId, true);
  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  UninstallApp(kTestOtherExtensionId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget_);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Exclude the test from ChromeOS because profile destruction does not happen
// on ChromeOS in production.
//
// Tests that the dialog closes when the current profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedProfileClosesDialog) {
  ShowAppInfo(kTestExtensionId);
  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());

  // Delete the profile.
  extension_environment_.DeleteProfile();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget_);
}

// Tests that the dialog does not close when a different profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedOtherProfileDoesNotCloseDialog) {
  ShowAppInfo(kTestExtensionId);
  std::unique_ptr<TestingProfile> other_profile(new TestingProfile);
  extension_environment_.CreateExtensionServiceForProfile(other_profile.get());

  scoped_refptr<const extensions::Extension> other_app =
      extension_environment_.MakePackagedApp(kTestOtherExtensionId, false);
  extensions::ExtensionRegistrar::Get(other_profile.get())
      ->AddExtension(other_app.get());

  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  other_profile.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget_);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(AppInfoDialogViewsTest, ArcAppInfoLinks) {
  ShowAppInfo(app_constants::kChromeAppId);
  EXPECT_FALSE(widget_->IsClosed());
  // App Info should not have ARC App info links section because ARC Settings
  // app is not available yet.
  EXPECT_FALSE(dialog_->arc_app_info_links_for_test());

  // Re-show App Info but with ARC Settings app enabled.
  CloseAppInfo();
  ArcAppListPrefs* arc_prefs =
      ArcAppListPrefs::Get(extension_environment_.profile());
  ASSERT_TRUE(arc_prefs);
  arc::mojom::AppHost* app_host = arc_prefs;
  app_host->OnAppListRefreshed(GetArcSettingsAppInfo());
  EXPECT_TRUE(arc_prefs->IsRegistered(arc::kSettingsAppId));
  ShowAppInfo(app_constants::kChromeAppId);
  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_TRUE(dialog_->arc_app_info_links_for_test());

  // Re-show App Info but for non-primary profile.
  CloseAppInfo();
  const AccountId other_account_id =
      AccountId::FromUserEmail("other_profile@gmail.com");
  auto* fake_user_manager = static_cast<ash::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
  fake_user_manager->AddUser(other_account_id);

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  ash::ScopedAccountIdAnnotator annotator(profile_manager.profile_manager(),
                                          other_account_id);
  TestingProfile* other_profile =
      profile_manager.CreateTestingProfile("other_profile@gmail.com");

  extension_environment_.CreateExtensionServiceForProfile(other_profile);
  // We're adding the extension to the second profile, so don't install it
  // automatically in the profile from `extension_environment_`.
  const bool install = false;
  scoped_refptr<const extensions::Extension> other_app =
      extension_environment_.MakePackagedApp(app_constants::kChromeAppId,
                                             install);
  extensions::ExtensionRegistrar::Get(other_profile)
      ->AddExtension(other_app.get());
  ShowAppInfoForProfile(app_constants::kChromeAppId, other_profile);
  EXPECT_FALSE(widget_->IsClosed());
  // The ARC App info links are not available if ARC is not allowed for
  // secondary profile.
  EXPECT_FALSE(dialog_->arc_app_info_links_for_test());
  CloseAppInfo();
}

// Tests that the pin/unpin button is focused after unpinning/pinning. This is
// to verify regression in crbug.com/41140316 is fixed.
TEST_F(AppInfoDialogViewsTest, PinButtonsAreFocusedAfterPinUnpin) {
  ShowAppInfo(kTestExtensionId);
  AppInfoFooterPanel* dialog_footer =
      static_cast<AppInfoFooterPanel*>(dialog_->dialog_footer_);
  views::View* pin_button = dialog_footer->pin_to_shelf_button_;
  views::View* unpin_button = dialog_footer->unpin_from_shelf_button_;

  pin_button->RequestFocus();
  EXPECT_TRUE(pin_button->GetVisible());
  EXPECT_FALSE(unpin_button->GetVisible());
  EXPECT_TRUE(pin_button->HasFocus());

  // Avoid attempting to use sync, it's not initialized in this test.
  auto sync_disabler = chrome_shelf_controller_->GetScopedPinSyncDisabler();

  dialog_footer->SetPinnedToShelf(true);
  EXPECT_FALSE(pin_button->GetVisible());
  EXPECT_TRUE(unpin_button->GetVisible());
  EXPECT_TRUE(unpin_button->HasFocus());

  dialog_footer->SetPinnedToShelf(false);
  EXPECT_TRUE(pin_button->GetVisible());
  EXPECT_FALSE(unpin_button->GetVisible());
  EXPECT_TRUE(pin_button->HasFocus());
}
#endif
