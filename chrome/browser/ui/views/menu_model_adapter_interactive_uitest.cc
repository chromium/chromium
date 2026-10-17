// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_model_adapter.h"

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/views/menu_test_base.h"
#include "chrome/browser/ui/views/test/view_event_test_base.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace {

const int kTopMenuBaseId = 100;
const int kSubMenuBaseId = 200;

// Implement most of the ui::MenuModel pure virtual methods for subclasses
//
// Exceptions:
//  virtual int GetItemCount() const = 0;
//  virtual ItemType GetTypeAt(int index) const = 0;
//  virtual int GetCommandIdAt(int index) const = 0;
//  virtual std::u16string GetLabelAt(int index) const = 0;
class CommonMenuModel : public ui::MenuModel {
 public:
  CommonMenuModel() = default;

  CommonMenuModel(const CommonMenuModel&) = delete;
  CommonMenuModel& operator=(const CommonMenuModel&) = delete;

  ~CommonMenuModel() override = default;

 protected:
  // ui::MenuModel:
  bool IsItemDynamicAt(size_t index) const override { return false; }

  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  bool IsItemCheckedAt(size_t index) const override { return false; }

  int GetGroupIdAt(size_t index) const override { return 0; }

  ui::ImageModel GetIconAt(size_t index) const override {
    return ui::ImageModel();
  }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override {
    return nullptr;
  }

  bool IsEnabledAt(size_t index) const override { return true; }

  ui::MenuModel* GetSubmenuModelAt(size_t index) const override {
    return nullptr;
  }

  void ActivatedAt(size_t index) override {}
};

class SubMenuModel final : public CommonMenuModel {
 public:
  SubMenuModel() = default;

  SubMenuModel(const SubMenuModel&) = delete;
  SubMenuModel& operator=(const SubMenuModel&) = delete;

  ~SubMenuModel() override = default;

  bool showing() const { return showing_; }

 private:
  // ui::MenuModel implementation.
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  size_t GetItemCount() const override { return 1; }

  ItemType GetTypeAt(size_t index) const override { return TYPE_COMMAND; }

  int GetCommandIdAt(size_t index) const override {
    return static_cast<int>(index) + kSubMenuBaseId;
  }

  std::u16string GetLabelAt(size_t index) const override { return u"Item"; }

  void MenuWillShow() override { showing_ = true; }

  // Called when the menu is about to close.
  void MenuWillClose() override { showing_ = false; }

  bool showing_ = false;
  base::WeakPtrFactory<SubMenuModel> weak_ptr_factory_{this};
};

class TopMenuModel final : public CommonMenuModel {
 public:
  TopMenuModel() = default;

  TopMenuModel(const TopMenuModel&) = delete;
  TopMenuModel& operator=(const TopMenuModel&) = delete;

  ~TopMenuModel() override = default;

  bool IsSubmenuShowing() { return sub_menu_model_.showing(); }

 private:
  // ui::MenuModel implementation.
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  size_t GetItemCount() const override { return 1; }

  ItemType GetTypeAt(size_t index) const override { return TYPE_SUBMENU; }

  int GetCommandIdAt(size_t index) const override {
    return static_cast<int>(index) + kTopMenuBaseId;
  }

  std::u16string GetLabelAt(size_t index) const override { return u"submenu"; }

  MenuModel* GetSubmenuModelAt(size_t index) const override {
    return &sub_menu_model_;
  }

  mutable SubMenuModel sub_menu_model_;
  base::WeakPtrFactory<TopMenuModel> weak_ptr_factory_{this};
};

}  // namespace

class MenuModelAdapterTest : public MenuTestBase {
 public:
  MenuModelAdapterTest() = default;
  ~MenuModelAdapterTest() override = default;

  // MenuTestBase:
  void BuildMenu(views::MenuItemView* menu) override {
    // The adapter must be the delegate so that ui::MenuModel receives
    // MenuWillShow()/MenuWillClose() notifications.
    menu->set_delegate(&menu_model_adapter_);
    menu_model_adapter_.BuildMenu(menu);
  }

  void DoTestWithMenuOpen() override {
    views::SubmenuView* topmenu = menu()->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_TRUE(topmenu->IsShowing());
    ASSERT_FALSE(top_menu_model_.IsSubmenuShowing());

    // Click the first item to open the submenu.
    views::MenuItemView* item = topmenu->GetMenuItemAt(0);
    ASSERT_TRUE(item);
    Click(item, CreateEventTask(this, &MenuModelAdapterTest::Step2));
  }

  // Rebuild the menu which should close the submenu.
  void Step2() {
    views::SubmenuView* topmenu = menu()->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_TRUE(topmenu->IsShowing());
    ASSERT_TRUE(top_menu_model_.IsSubmenuShowing());

    menu_model_adapter_.BuildMenu(menu());

    ASSERT_TRUE(base::CurrentUIThread::IsSet());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, CreateEventTask(this, &MenuModelAdapterTest::Step3));
  }

  // Verify that the submenu MenuModel received the close callback
  // and close the menu.
  void Step3() {
    views::SubmenuView* topmenu = menu()->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_TRUE(topmenu->IsShowing());
    ASSERT_FALSE(top_menu_model_.IsSubmenuShowing());

    // Click the button to exit the menu.
    Click(button(), CreateEventTask(this, &MenuModelAdapterTest::Step4));
  }

  // All done.
  void Step4() {
    views::SubmenuView* topmenu = menu()->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_FALSE(topmenu->IsShowing());
    ASSERT_FALSE(top_menu_model_.IsSubmenuShowing());

    Done();
  }

 private:
  TopMenuModel top_menu_model_;
  views::MenuModelAdapter menu_model_adapter_{&top_menu_model_};
};

// If this flakes, disable and log details in http://crbug.com/40432443.
VIEW_TEST(MenuModelAdapterTest, RebuildMenu)
