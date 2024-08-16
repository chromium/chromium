// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_model_adapter.h"

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/views/test/view_event_test_base.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/test/ui_controls.h"
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
//  virtual std::u16string GetLabelAt(int index) const = 0;
class CommonMenuModel : public ui::MenuModel {
 public:
  CommonMenuModel() {
  }

  CommonMenuModel(const CommonMenuModel&) = delete;
  CommonMenuModel& operator=(const CommonMenuModel&) = delete;

  ~CommonMenuModel() override {}

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

  ~SubMenuModel() override {}

  bool showing() const {
    return showing_;
  }

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
  TopMenuModel() {
  }

  TopMenuModel(const TopMenuModel&) = delete;
  TopMenuModel& operator=(const TopMenuModel&) = delete;

  ~TopMenuModel() override {}

  bool IsSubmenuShowing() {
    return sub_menu_model_.showing();
  }

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

class MenuModelAdapterTest : public ViewEventTestBase {
 public:
  MenuModelAdapterTest() = default;
  ~MenuModelAdapterTest() override = default;

  // ViewEventTestBase implementation.

  void SetUp() override {
    ViewEventTestBase::SetUp();

    std::unique_ptr<views::MenuItemView> menu =
        menu_model_adapter_.CreateMenu();
    menu_ = menu.get();
    menu_runner_ = std::make_unique<views::MenuRunner>(
        std::move(menu), views::MenuRunner::HAS_MNEMONICS);
  }

  void TearDown() override {
    menu_ = nullptr;
    menu_runner_.reset();

    button_ = nullptr;
    ViewEventTestBase::TearDown();
  }

  std::unique_ptr<views::View> CreateContentsView() override {
    auto button = std::make_unique<views::MenuButton>(
        base::BindRepeating(&MenuModelAdapterTest::ButtonPressed,
                            base::Unretained(this)),
        u"Menu Adapter Test");
    button_ = button.get();
    return button;
  }

  gfx::Size GetPreferredSizeForContents() const override {
    return button_->GetPreferredSize();
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

    ASSERT_TRUE(base::CurrentUIThread::IsSet());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

  void ButtonPressed() {
    menu_runner_->RunMenuAt(button_->GetWidget(), button_->button_controller(),
                            button_->GetBoundsInScreen(),
                            views::MenuAnchorPosition::kTopLeft,
                            ui::MENU_SOURCE_NONE);
  }

  raw_ptr<views::MenuButton> button_ = nullptr;
  TopMenuModel top_menu_model_;
  views::MenuModelAdapter menu_model_adapter_{&top_menu_model_};
  std::unique_ptr<views::MenuRunner> menu_runner_;
  raw_ptr<views::MenuItemView> menu_ = nullptr;
};

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuModelAdapterTest, RebuildMenu)
