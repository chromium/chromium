// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/shelf_model.h"  // nogncheck
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#endif

#if defined(OS_CHROMEOS)
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
#endif

namespace test {

class AppInfoDialogTestApi {
 public:
  explicit AppInfoDialogTestApi(AppInfoDialog* dialog) : dialog_(dialog) {}

  AppInfoHeaderPanel* header_panel() {
    return static_cast<AppInfoHeaderPanel*>(dialog_->children().front());
  }

  views::Link* view_in_store_link() {
    return header_panel()->view_in_store_link_;
  }

 private:
  AppInfoDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialogTestApi);
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

  // Overridden from testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
#if defined(OS_CHROMEOS)
    shelf_model_ = std::make_unique<ash::ShelfModel>();
    chrome_launcher_controller_ = std::make_unique<ChromeLauncherController>(
        extension_environment_.profile(), shelf_model_.get());
    chrome_launcher_controller_->Init();
    arc_test_.SetUp(extension_environment_.profile());
#endif
    extension_ = extension_environment_.MakePackagedApp(kTestExtensionId, true);
    chrome_app_ = extension_environment_.MakePackagedApp(
        extension_misc::kChromeAppId, true);
  }

  void TearDown() override {
    CloseAppInfo();
    extension_ = nullptr;
    chrome_app_ = nullptr;
#if defined(OS_CHROMEOS)
    arc_test_.TearDown();
    chrome_launcher_controller_.reset();
    shelf_model_.reset();
#endif

    // The Browser class had dependencies on LocalState, which is owned by
    // |extension_environment_|.
    auto* browser = release_browser();
    if (browser) {
      browser->tab_strip_model()->CloseAllTabs();
      delete browser;
    }
    extension_environment_.DeleteProfile();

    BrowserWithTestWindowTest::TearDown();
  }

  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    return extension_environment_.profile();
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
    widget_ = NULL;
  }

  void UninstallApp(const std::string& app_id) {
    extensions::ExtensionSystem::Get(extension_environment_.profile())
        ->extension_service()
        ->UninstallExtension(
            app_id, extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING,
            NULL);
  }

 protected:
  views::Widget* widget_ = nullptr;
  AppInfoDialog* dialog_ = nullptr;  // Owned by |widget_|'s views hierarchy.
  scoped_refptr<const extensions::Extension> extension_;
  scoped_refptr<const extensions::Extension> chrome_app_;
  extensions::TestExtensionEnvironment extension_environment_{
      extensions::TestExtensionEnvironment::Type::
          kInheritExistingTaskEnvironment};
#if defined(OS_CHROMEOS)
  std::unique_ptr<ash::ShelfModel> shelf_model_;
  std::unique_ptr<ChromeLauncherController> chrome_launcher_controller_;
  ArcAppTest arc_test_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInfoDialogViewsTest);
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

  // First delete the test browser window. This ensures the test harness isn't
  // surprised by it being closed in response to the profile deletion below.
  std::unique_ptr<Browser> browser(release_browser());
  browser->tab_strip_model()->CloseAllTabs();
  browser.reset();
  std::unique_ptr<BrowserWindow> browser_window(release_browser_window());
  browser_window->Close();
  browser_window.reset();

#if defined(OS_CHROMEOS)
  chrome_launcher_controller_.reset();
  shelf_model_.reset();
  arc_test_.TearDown();
#endif

  // The following does nothing: it just ensures the Widget close is being
  // triggered by the DeleteProfile() call rather than the code above.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
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
  EXPECT_TRUE(extension_->from_webstore());  // Otherwise there is no link.
  views::Link* link = test::AppInfoDialogTestApi(dialog_).view_in_store_link();
  EXPECT_TRUE(link);

  TabStripModel* tabs = browser()->tab_strip_model();
  EXPECT_EQ(0, tabs->count());

  ASSERT_TRUE(widget_);
  EXPECT_FALSE(widget_->IsClosed());
  link->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0));

  ASSERT_TRUE(widget_);
  EXPECT_TRUE(widget_->IsClosed());

  EXPECT_EQ(1, tabs->count());
  content::WebContents* web_contents = tabs->GetWebContentsAt(0);

  std::string url = "https://chrome.google.com/webstore/detail/";
  url += kTestExtensionId;
  url += "?utm_source=chrome-app-launcher-info-dialog";
  EXPECT_EQ(GURL(url), web_contents->GetURL());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget_);
}

#if defined(OS_CHROMEOS)
TEST_F(AppInfoDialogViewsTest, ArcAppInfoLinks) {
  ShowAppInfo(extension_misc::kChromeAppId);
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
  ShowAppInfo(extension_misc::kChromeAppId);
  EXPECT_FALSE(widget_->IsClosed());
  EXPECT_TRUE(dialog_->arc_app_info_links_for_test());

  // Re-show App Info but for non-primary profile.
  CloseAppInfo();
  std::unique_ptr<TestingProfile> other_profile =
      std::make_unique<TestingProfile>();
  extension_environment_.CreateExtensionServiceForProfile(other_profile.get());
  scoped_refptr<const extensions::Extension> other_app =
      extension_environment_.MakePackagedApp(extension_misc::kChromeAppId,
                                             true);
  extensions::ExtensionSystem::Get(other_profile.get())
      ->extension_service()
      ->AddExtension(other_app.get());
  ShowAppInfoForProfile(extension_misc::kChromeAppId, other_profile.get());
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
  auto sync_disabler = chrome_launcher_controller_->GetScopedPinSyncDisabler();

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
