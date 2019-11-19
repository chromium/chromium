// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_model_adapter.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/views/test/view_event_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace {

const int kTopMenuBaseId = 100;
const int kSubMenuBaseId = 200;

// Implement most of the ui::MenuModel pure virtual methods for subclasses
//
// Exceptions:
//  virtual int GetItemCount() const = 0;
//  virtual ItemType GetTypeAt(int index) const = 0;
//  virtual int GetCommandIdAt(int index) const = 0;
//  virtual base::string16 GetLabelAt(int index) const = 0;
class CommonMenuModel : public ui::MenuModel {
 public:
  CommonMenuModel() {
  }

  ~CommonMenuModel() override {}

 protected:
  // ui::MenuModel implementation.
  bool HasIcons() const override { return false; }

  bool IsItemDynamicAt(int index) const override { return false; }

  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  bool IsItemCheckedAt(int index) const override { return false; }

  int GetGroupIdAt(int index) const override { return 0; }

  bool GetIconAt(int index, gfx::Image* icon) const override { return false; }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }

  bool IsEnabledAt(int index) const override { return true; }

  ui::MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }

  void ActivatedAt(int index) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CommonMenuModel);
};

class SubMenuModel : public CommonMenuModel {
 public:
  SubMenuModel()
      : showing_(false) {
  }

  ~SubMenuModel() override {}

  bool showing() const {
    return showing_;
  }

 private:
  // ui::MenuModel implementation.
  int GetItemCount() const override { return 1; }

  ItemType GetTypeAt(int index) const override { return TYPE_COMMAND; }

  int GetCommandIdAt(int index) const override {
    return index + kSubMenuBaseId;
  }

  base::string16 GetLabelAt(int index) const override {
    return base::ASCIIToUTF16("Item");
  }

  void MenuWillShow() override { showing_ = true; }

  // Called when the menu is about to close.
  void MenuWillClose() override { showing_ = false; }

  bool showing_;

  DISALLOW_COPY_AND_ASSIGN(SubMenuModel);
};

class TopMenuModel : public CommonMenuModel {
 public:
  TopMenuModel() {
  }

  ~TopMenuModel() override {}

  bool IsSubmenuShowing() {
    return sub_menu_model_.showing();
  }

 private:
  // ui::MenuModel implementation.
  int GetItemCount() const override { return 1; }

  ItemType GetTypeAt(int index) const override { return TYPE_SUBMENU; }

  int GetCommandIdAt(int index) const override {
    return index + kTopMenuBaseId;
  }

  base::string16 GetLabelAt(int index) const override {
    return base::ASCIIToUTF16("submenu");
  }

  MenuModel* GetSubmenuModelAt(int index) const override {
    return &sub_menu_model_;
  }

  mutable SubMenuModel sub_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(TopMenuModel);
};

}  // namespace

class MenuModelAdapterTest : public ViewEventTestBase,
                             public views::ButtonListener {
 public:
  MenuModelAdapterTest()
      : ViewEventTestBase(),
        button_(nullptr),
        menu_model_adapter_(&top_menu_model_),
        menu_(nullptr) {}

  ~MenuModelAdapterTest() override {}

  // ViewEventTestBase implementation.

  void SetUp() override {
    button_ =
        new views::MenuButton(base::ASCIIToUTF16("Menu Adapter Test"), this);

    menu_ = menu_model_adapter_.CreateMenu();
    menu_runner_.reset(
        new views::MenuRunner(menu_, views::MenuRunner::HAS_MNEMONICS));

    ViewEventTestBase::SetUp();
  }

  void TearDown() override {
    menu_runner_ = nullptr;
    menu_ = nullptr;
    ViewEventTestBase::TearDown();
  }

  views::View* CreateContentsView() override { return button_; }

  gfx::Size GetPreferredSizeForContents() const override {
    return button_->GetPreferredSize();
  }

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* source, const ui::Event& event) override {
    gfx::Point screen_location;
    views::View::ConvertPointToScreen(source, &screen_location);
    gfx::Rect bounds(screen_location, source->size());
    menu_runner_->RunMenuAt(source->GetWidget(), button_->button_controller(),
                            bounds, views::MenuAnchorPosition::kTopLeft,
                            ui::MENU_SOURCE_NONE);
  }

  // ViewEventTestBase implementation
  void DoTestOnMessageLoop() override {
    Click(button_, CreateEventTask(this, &MenuModelAdapterTest::Step1));
  }

  // Open the submenu.
  void Step1() {
    views::test::DisableMenuClosureAnimations();
    views::SubmenuView* topmenu = menu_->GetSubmenu();
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
    views::SubmenuView* topmenu = menu_->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_TRUE(topmenu->IsShowing());
    ASSERT_TRUE(top_menu_model_.IsSubmenuShowing());

    menu_model_adapter_.BuildMenu(menu_);

    ASSERT_TRUE(base::MessageLoopCurrentForUI::IsSet());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, CreateEventTask(this, &MenuModelAdapterTest::Step3));
  }

  // Verify that the submenu MenuModel received the close callback
  // and close the menu.
  void Step3() {
    views::SubmenuView* topmenu = menu_->GetSubmenu();
    ASSERT_TRUE(topmenu);
    ASSERT_TRUE(topmenu->IsShowing());
    ASSERT_FALSE(top_menu_model_.IsSubmenuShowing());

    // Click the button to exit the menu.
    Click(button_, CreateEventTask(this, &MenuModelAdapterTest::Step4));
  }

  // All done.
  void Step4() {
    views::SubmenuView* topmenu = menu_->GetSubmenu();
    views::test::WaitForMenuClosureAnimation();
    ASSERT_TRUE(topmenu);
    ASSERT_FALSE(topmenu->IsShowing());
    ASSERT_FALSE(top_menu_model_.IsSubmenuShowing());

    Done();
  }

 private:
  // Generate a mouse click on the specified view and post a new task.
  virtual void Click(views::View* view, base::OnceClosure next) {
    ui_test_utils::MoveMouseToCenterAndPress(
        view, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        std::move(next));
  }

  views::MenuButton* button_;
  TopMenuModel top_menu_model_;
  views::MenuModelAdapter menu_model_adapter_;
  views::MenuItemView* menu_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuModelAdapterTest, RebuildMenu)
