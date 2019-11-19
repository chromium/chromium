// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/default_theme_provider.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Error class has a menu item.
class MenuError : public GlobalError {
 public:
  explicit MenuError(int command_id)
      : command_id_(command_id),
        execute_count_(0) {
  }

  int execute_count() { return execute_count_; }

  bool HasMenuItem() override { return true; }
  int MenuItemCommandID() override { return command_id_; }
  base::string16 MenuItemLabel() override { return base::string16(); }
  void ExecuteMenuItem(Browser* browser) override { execute_count_++; }

  bool HasBubbleView() override { return false; }
  bool HasShownBubbleView() override { return false; }
  void ShowBubbleView(Browser* browser) override { ADD_FAILURE(); }
  GlobalErrorBubbleViewBase* GetBubbleView() override { return NULL; }

 private:
  int command_id_;
  int execute_count_;

  DISALLOW_COPY_AND_ASSIGN(MenuError);
};

class FakeIconDelegate : public AppMenuIconController::Delegate {
 public:
  FakeIconDelegate() = default;

  // AppMenuIconController::Delegate:
  void UpdateTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity) override {}
  const ui::ThemeProvider* GetViewThemeProvider() const override {
    return &theme_provider_;
  }
  ui::NativeTheme* GetViewNativeTheme() override {
    return ui::NativeTheme::GetInstanceForNativeUi();
  }

 private:
  ui::DefaultThemeProvider theme_provider_;
};

} // namespace

class AppMenuModelTest : public BrowserWithTestWindowTest,
                         public ui::AcceleratorProvider {
 public:
  AppMenuModelTest() = default;
  ~AppMenuModelTest() override = default;

  // Don't handle accelerators.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppMenuModelTest);
};

// Copies parts of MenuModelTest::Delegate and combines them with the
// AppMenuModel since AppMenuModel is now a SimpleMenuModel::Delegate and
// not derived from SimpleMenuModel.
class TestAppMenuModel : public AppMenuModel {
 public:
  TestAppMenuModel(ui::AcceleratorProvider* provider,
                   Browser* browser,
                   AppMenuIconController* app_menu_icon_controller)
      : AppMenuModel(provider, browser, app_menu_icon_controller),
        execute_count_(0),
        checked_count_(0),
        enable_count_(0) {}

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    bool val = AppMenuModel::IsCommandIdChecked(command_id);
    if (val)
      checked_count_++;
    return val;
  }

  bool IsCommandIdEnabled(int command_id) const override {
    ++enable_count_;
    return true;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    ++execute_count_;
  }

  int execute_count_;
  mutable int checked_count_;
  mutable int enable_count_;
};

TEST_F(AppMenuModelTest, Basics) {
  // Simulate that an update is available to ensure that the menu includes the
  // upgrade item for platforms that support it.
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  detector->set_upgrade_notification_stage(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  detector->NotifyUpgrade();
  EXPECT_TRUE(detector->notify_upgrade());

  FakeIconDelegate fake_delegate;
  AppMenuIconController app_menu_icon_controller(browser()->profile(),
                                                 &fake_delegate);
  TestAppMenuModel model(this, browser(), &app_menu_icon_controller);
  model.Init();
  int itemCount = model.GetItemCount();

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(itemCount, 10);

  // Verify that the upgrade item is visible if supported.
  EXPECT_EQ(browser_defaults::kShowUpgradeMenuItem,
            model.IsCommandIdVisible(IDC_UPGRADE_DIALOG));

  // Execute a couple of the items and make sure it gets back to our delegate.
  // We can't use CountEnabledExecutable() here because the encoding menu's
  // delegate is internal, it doesn't use the one we pass in.
  // Note: the second item in the menu may be a separator if the browser
  // supports showing upgrade status in the app menu.
  int item_index = 1;
  if (model.GetTypeAt(item_index) == ui::MenuModel::TYPE_SEPARATOR)
    ++item_index;
  model.ActivatedAt(item_index);
  EXPECT_TRUE(model.IsEnabledAt(item_index));
  // Make sure to use the index that is not separator in all configurations.
  model.ActivatedAt(itemCount - 1);
  EXPECT_TRUE(model.IsEnabledAt(itemCount - 1));

  EXPECT_EQ(model.execute_count_, 2);
  EXPECT_EQ(model.enable_count_, 2);

  model.execute_count_ = 0;
  model.enable_count_ = 0;

  // Choose something from the bookmark submenu and make sure it makes it back
  // to the delegate as well.
  int bookmarksModelIndex = -1;
  for (int i = 0; i < itemCount; ++i) {
    if (model.GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      // The bookmarks submenu comes after the Tabs and Downloads items.
      bookmarksModelIndex = i + 2;
      break;
    }
  }
  EXPECT_GT(bookmarksModelIndex, -1);
  ui::MenuModel* bookmarksModel = model.GetSubmenuModelAt(bookmarksModelIndex);
  EXPECT_TRUE(bookmarksModel);
  // The bookmarks model may be empty until we tell it we're going to show it.
  bookmarksModel->MenuWillShow();
  EXPECT_GT(bookmarksModel->GetItemCount(), 1);

  // Bookmark manager item.
  bookmarksModel->ActivatedAt(4);
  EXPECT_TRUE(bookmarksModel->IsEnabledAt(4));
  EXPECT_EQ(model.execute_count_, 1);
  EXPECT_EQ(model.enable_count_, 1);
}

// Tests global error menu items in the app menu.
TEST_F(AppMenuModelTest, GlobalError) {
  // Make sure services required for tests are initialized.
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser()->profile());
  const int command1 = 1234567;
  MenuError* error1 = new MenuError(command1);
  service->AddGlobalError(base::WrapUnique(error1));
  const int command2 = 1234568;
  MenuError* error2 = new MenuError(command2);
  service->AddGlobalError(base::WrapUnique(error2));

  AppMenuModel model(this, browser());
  model.Init();
  int index1 = model.GetIndexOfCommandId(command1);
  EXPECT_GT(index1, -1);
  int index2 = model.GetIndexOfCommandId(command2);
  EXPECT_GT(index2, -1);

  EXPECT_TRUE(model.IsEnabledAt(index1));
  EXPECT_EQ(0, error1->execute_count());
  model.ActivatedAt(index1);
  EXPECT_EQ(1, error1->execute_count());

  EXPECT_TRUE(model.IsEnabledAt(index2));
  EXPECT_EQ(0, error2->execute_count());
  model.ActivatedAt(index2);
  EXPECT_EQ(1, error1->execute_count());
}
