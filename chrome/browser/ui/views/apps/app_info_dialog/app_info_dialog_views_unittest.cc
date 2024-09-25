// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/app_constants/constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/public/cpp/shelf_model.h"  // nogncheck
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace test {

class AppInfoDialogTestApi {
 public:
  explicit AppInfoDialogTestApi(AppInfoDialog* dialog) : dialog_(dialog) {}

  AppInfoDialogTestApi(const AppInfoDialogTestApi&) = delete;
  AppInfoDialogTestApi& operator=(const AppInfoDialogTestApi&) = delete;

  void ShowAppInWebStore() {
    auto* header_panel =
        static_cast<AppInfoHeaderPanel*>(dialog_->children().front());
    return header_panel->ShowAppInWebStore();
  }

 private:
  raw_ptr<AppInfoDialog> dialog_;
};

}  // namespace test

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestOtherExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

}  // namespace

class AppInfoDialogViewsTest : public BrowserWithTestWindowTest,
                               public views::WidgetObserver {
 public:
  AppInfoDialogViewsTest() = default;

  AppInfoDialogViewsTest(const AppInfoDialogViewsTest&) = delete;
  AppInfoDialogViewsTest& operator=(const AppInfoDialogViewsTest&) = delete;

  // Overridden from testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Sets up a fake user manager over |BrowserWithTestWindowTest| user
    // manager.
    arc_test_ =
        std::make_unique<ArcAppTest>(ArcAppTest::UserManagerMode::kDoNothing);
    arc_test_->SetUp(extension_environment_.profile());

    shelf_model_ = std::make_unique<ash::ShelfModel>();
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chrome_shelf_controller_.reset();
    shelf_model_.reset();
    if (arc_test_) {
      arc_test_->TearDown();
      arc_test_.reset();
    }
#endif

    // The Browser class had dependencies on LocalState, which is owned by
    // |extension_environment_|.
    std::unique_ptr<Browser> browser = release_browser();
    if (browser) {
      browser->tab_strip_model()->CloseAllTabs();
      browser.reset();
      // Browser holds a ScopedProfileKeepAlive, which might post a task to the
      // UI thread on destruction.
      base::RunLoop().RunUntilIdle();
    }
    extension_environment_.DeleteProfile();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    auto* profile = BrowserWithTestWindowTest::CreateProfile(profile_name);
    extension_environment_.SetProfile(profile);
    return profile;
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
        new views::DialogDelegateView(), GetContext(), nullptr);
    widget_->AddObserver(this);
    dialog_ = widget_->GetContentsView()->AddChildView(
        std::make_unique<AppInfoDialog>(profile, extension));
    widget_->Show();
  }

  void CloseAppInfo() {
    if (widget_)
      widget_->CloseNow();
    base::RunLoop().RunUntilIdle();
    DCHECK(!widget_);
  }

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }

  void UninstallApp(const std::string& app_id) {
    extensions::ExtensionSystem::Get(extension_environment_.profile())
        ->extension_service()
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
      extensions::TestExtensionEnvironment::ProfileCreationType::kNoCreate,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      extensions::TestExtensionEnvironment::OSSetupType::kNoSetUp,
#endif
  };
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::ShelfModel> shelf_model_;
  std::unique_ptr<ChromeShelfController> chrome_shelf_controller_;
  std::unique_ptr<ArcAppTest> arc_test_;
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

// Tests that the dialog closes when the current profile is destroyed.
TEST_F(AppInfoDialogViewsTest, DestroyedProfileClosesDialog) {
  ShowAppInfo(kTestExtensionId);

  {
    // Prevent from unexpected profile deletion due to browser deletion.
    ScopedProfileKeepAlive keep_alive(
        profile(), ProfileKeepAliveOrigin::kProfileDeletionProcess);

    // First delete the test browser window. This ensures the test harness isn't
    // surprised by it being closed in response to the profile deletion below.
    std::unique_ptr<Browser> browser = release_browser();
    browser->tab_strip_model()->CloseAllTabs();
    browser.reset();
    std::unique_ptr<BrowserWindow> browser_window = release_browser_window();
    browser_window->Close();
    browser_window.reset();

    // The following serves two purposes:
    // it ensures the Widget close is being triggered by the DeleteProfile()
    // call rather than the code above. And prevents a race condition while
    // tearing down arc_test user_manager.
    base::RunLoop().RunUntilIdle();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Avoid a race condition when tearing down arc_test_ and deleting the user
    // manager.
    chrome_shelf_controller_.reset();
    shelf_model_.reset();
    arc_test_->TearDown();
    arc_test_.reset();
#endif

    ASSERT_TRUE(widget_);
    EXPECT_FALSE(widget_->IsClosed());
  }

  // Delete the profile.
  extension_environment_.DeleteProfile();
  // ScopedProfileKeepAlive's destruction may post a task to the current
  // sequence, which needs the profile. So, before calling DeleteProfile,
  // RunLoop once again.
  base::RunLoop().RunUntilIdle();
  // Note: On platforms except ChromeOS, the above RunLoop will delete the
  // profile. On ChromeOS (i.e. Ash-Chrome), profile won't be delete by that
  // because even if all browsers are closed Profile is expected to be kept
  // for system. Explicitly delete it here.
  DeleteProfile(GetDefaultProfileName());
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
  extensions::ExtensionSystem::Get(other_profile.get())
      ->extension_service()
      ->AddExtension(other_app.get());

  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  other_profile.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget_);
}

// Tests that clicking the View in Store link opens a browser tab and closes the
// dialog cleanly.
TEST_F(AppInfoDialogViewsTest, ViewInStore) {
  ShowAppInfo(kTestExtensionId);
  ASSERT_TRUE(extension_->from_webstore());

  TabStripModel* tabs = browser()->tab_strip_model();
  EXPECT_EQ(0, tabs->count());

  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  test::AppInfoDialogTestApi(dialog_).ShowAppInWebStore();

  ASSERT_TRUE(widget_);
  EXPECT_TRUE(widget_->IsClosed());

  EXPECT_EQ(1, tabs->count());
  content::WebContents* web_contents = tabs->GetWebContentsAt(0);

  std::string url = extension_urls::GetWebstoreItemDetailURLPrefix();
  url += kTestExtensionId;
  url += "?utm_source=chrome-app-launcher-info-dialog";
  EXPECT_EQ(GURL(url), web_contents->GetURL());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  std::unique_ptr<TestingProfile> other_profile =
      std::make_unique<TestingProfile>();
  extension_environment_.CreateExtensionServiceForProfile(other_profile.get());
  // We're adding the extension to the second profile, so don't install it
  // automatically in the profile from `extension_environment_`.
  const bool install = false;
  scoped_refptr<const extensions::Extension> other_app =
      extension_environment_.MakePackagedApp(app_constants::kChromeAppId,
                                             install);
  extensions::ExtensionSystem::Get(other_profile.get())
      ->extension_service()
      ->AddExtension(other_app.get());
  ShowAppInfoForProfile(app_constants::kChromeAppId, other_profile.get());
  EXPECT_FALSE(widget_->IsClosed());
  // The ARC App info links are not available if ARC is not allowed for
  // secondary profile.
  EXPECT_FALSE(dialog_->arc_app_info_links_for_test());
}

// Tests that the pin/unpin button is focused after unpinning/pinning. This is
// to verify regression in crbug.com/428704 is fixed.
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
