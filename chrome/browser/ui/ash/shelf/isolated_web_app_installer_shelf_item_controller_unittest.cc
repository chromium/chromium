// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/isolated_web_app_installer_shelf_item_controller.h"

#include <memory>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/widget/widget.h"

class IsolatedWebAppInstallerShelfItemControllerTest : public ash::AshTestBase {
 public:
  void CreateWindowAndAddToShelf(const std::string& app_id) {
    widget_ = CreateTestWidget(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
        ash::desks_util::GetActiveDeskContainerId(), gfx::Rect());

    ash::ShelfID shelf_id(app_id);
    std::unique_ptr<IsolatedWebAppInstallerShelfItemController> delegate =
        std::make_unique<IsolatedWebAppInstallerShelfItemController>(shelf_id);
    ash::ShelfItem item;
    item.id = shelf_id;
    item.status = ash::STATUS_RUNNING;
    item.type = ash::TYPE_APP;

    shelf_model()->Add(item, std::move(delegate));

    auto* delegate_ptr = shelf_model()->GetShelfItemDelegate(shelf_id);
    ASSERT_TRUE(delegate_ptr);
    static_cast<LacrosShelfItemController*>(delegate_ptr)->AddWindow(window());
  }

  ash::ShelfModel* shelf_model() { return ash::ShelfModel::Get(); }

  aura::Window* window() { return widget_->GetNativeWindow(); }

  void TearDown() override {
    shelf_model()->DestroyItemDelegates();
    ash::AshTestBase::TearDown();
  }

 private:
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(IsolatedWebAppInstallerShelfItemControllerTest, OnItemSelected) {
  CreateWindowAndAddToShelf("test_id");
  window()->Hide();
  ASSERT_FALSE(window()->IsVisible());

  int index = shelf_model()->ItemIndexByAppID("test_id");
  ASSERT_NE(index, -1);
  ash::Shelf::ActivateShelfItemOnDisplay(index, display::kInvalidDisplayId);

  EXPECT_TRUE(window()->IsVisible());
}

TEST_F(IsolatedWebAppInstallerShelfItemControllerTest, GetContextMenu) {
  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> test_future;
  CreateWindowAndAddToShelf("test_id");

  shelf_model()
      ->GetShelfItemDelegate(ash::ShelfID("test_id"))
      ->GetContextMenu(/*display_id=*/0, test_future.GetCallback());
  std::unique_ptr<ui::SimpleMenuModel> menu = test_future.Take();

  ASSERT_TRUE(menu);
  EXPECT_EQ(menu->GetItemCount(), 1ul);
  EXPECT_EQ(menu->GetCommandIdAt(0), ash::MENU_CLOSE);
}

class TestObserver : public aura::WindowObserver {
 public:
  explicit TestObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  void OnWindowDestroyed(aura::Window* window) override {
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

TEST_F(IsolatedWebAppInstallerShelfItemControllerTest, ExecuteClosingCommand) {
  CreateWindowAndAddToShelf("test_id");
  base::test::TestFuture<void> future;

  TestObserver observer(future.GetCallback());
  window()->AddObserver(&observer);

  shelf_model()
      ->GetShelfItemDelegate(ash::ShelfID("test_id"))
      ->ExecuteCommand(/*from_context_menu=*/false, ash::MENU_CLOSE,
                       ui::EF_NONE, display::kInvalidDisplayId);

  ASSERT_TRUE(future.Wait());
}
