// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/menu_test_base.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

using base::ASCIIToUTF16;

// Simple test for clicking a menu item.  This template class clicks on an
// item and checks that the returned id matches.  The index of the item
// is the template argument.
template <int INDEX>
class MenuItemViewTestBasic : public MenuTestBase {
 public:
  MenuItemViewTestBasic() = default;
  MenuItemViewTestBasic(const MenuItemViewTestBasic&) = delete;
  MenuItemViewTestBasic& operator=(const MenuItemViewTestBasic&) = delete;
  ~MenuItemViewTestBasic() override = default;

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    menu->AppendMenuItem(1, u"item 1");
    menu->AppendMenuItem(2, u"item 2");
    menu->AppendSeparator();
    menu->AppendMenuItem(3, u"item 3");
  }

  // Click on item INDEX.
  void DoTestWithMenuOpen() override {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(3u, submenu->GetMenuItems().size());

    // click an item and pass control to the next step
    views::MenuItemView* item = submenu->GetMenuItemAt(INDEX);
    ASSERT_TRUE(item);
    Click(item, CreateEventTask(this, &MenuItemViewTestBasic::Step2));
  }

  // Check the clicked item and complete the test.
  void Step2() {
    ASSERT_FALSE(menu()->GetSubmenu()->IsShowing());
    ASSERT_EQ(INDEX + 1, last_command());
    Done();
  }
};

// Click each item of a 3-item menu (with separator).
using MenuItemViewTestBasic0 = MenuItemViewTestBasic<0>;
using MenuItemViewTestBasic1 = MenuItemViewTestBasic<1>;
using MenuItemViewTestBasic2 = MenuItemViewTestBasic<2>;
// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestBasic0, SelectItem0)
VIEW_TEST(MenuItemViewTestBasic1, SelectItem1)
VIEW_TEST(MenuItemViewTestBasic2, SelectItem2)

// Test class for inserting a menu item while the menu is open.
template <int INSERT_INDEX, int SELECT_INDEX>
class MenuItemViewTestInsert : public MenuTestBase {
 public:
  MenuItemViewTestInsert() = default;
  MenuItemViewTestInsert(const MenuItemViewTestInsert&) = delete;
  MenuItemViewTestInsert& operator=(const MenuItemViewTestInsert&) = delete;
  ~MenuItemViewTestInsert() override = default;

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    menu->AppendMenuItem(1, u"item 1");
    menu->AppendMenuItem(2, u"item 2");
  }

  // Insert item at INSERT_INDEX and click item at SELECT_INDEX.
  void DoTestWithMenuOpen() override {
    LOG(ERROR) << "\nDoTestWithMenuOpen\n";
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(2u, submenu->GetMenuItems().size());

    inserted_item_ = menu()->AddMenuItemAt(
        INSERT_INDEX, 1000, u"inserted item", std::u16string(),
        std::u16string(), ui::ImageModel(), ui::ImageModel(),
        views::MenuItemView::Type::kNormal, ui::NORMAL_SEPARATOR);
    ASSERT_TRUE(inserted_item_);
    menu()->ChildrenChanged();

    // click an item and pass control to the next step
    views::MenuItemView* item = submenu->GetMenuItemAt(SELECT_INDEX);
    ASSERT_TRUE(item);
    Click(item, CreateEventTask(this, &MenuItemViewTestInsert::Step2));
  }

  // Check clicked item and complete test.
  void Step2() {
    LOG(ERROR) << "\nStep2\n";
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_FALSE(submenu->IsShowing());
    ASSERT_EQ(3u, submenu->GetMenuItems().size());

    if (SELECT_INDEX == INSERT_INDEX)
      ASSERT_EQ(1000, last_command());
    else if (SELECT_INDEX < INSERT_INDEX)
      ASSERT_EQ(SELECT_INDEX + 1, last_command());
    else
      ASSERT_EQ(SELECT_INDEX, last_command());

    LOG(ERROR) << "\nDone\n";
    Done();
  }

 private:
  raw_ptr<views::MenuItemView, AcrossTasksDanglingUntriaged> inserted_item_ =
      nullptr;
};

// MenuItemViewTestInsertXY inserts an item at index X and selects the
// item at index Y (after the insertion).  The tests here cover
// inserting at the beginning, middle, and end, crossbarred with
// selecting the first and last item.
using MenuItemViewTestInsert00 = MenuItemViewTestInsert<0, 0>;
using MenuItemViewTestInsert02 = MenuItemViewTestInsert<0, 2>;
using MenuItemViewTestInsert10 = MenuItemViewTestInsert<1, 0>;
using MenuItemViewTestInsert12 = MenuItemViewTestInsert<1, 2>;
using MenuItemViewTestInsert20 = MenuItemViewTestInsert<2, 0>;
using MenuItemViewTestInsert22 = MenuItemViewTestInsert<2, 2>;

// If this flakes, disable and log details in http://crbug.com/523255.
#if defined(MEMORY_SANITIZER)
#define MAYBE_InsertItem00 DISABLED_InsertItem00
#else
#define MAYBE_InsertItem00 InsertItem00
#endif
VIEW_TEST(MenuItemViewTestInsert00, MAYBE_InsertItem00)

// TODO(b/523255): Test is failing consistently on "Linux Tests (Wayland)".
// If this flakes, disable and log details in http://crbug.com/523255.
// #if defined(MEMORY_SANITIZER)
// #define MAYBE_InsertItem02 DISABLED_InsertItem02
// #else
// #define MAYBE_InsertItem02 InsertItem02
// #endif
VIEW_TEST(MenuItemViewTestInsert02, DISABLED_InsertItem02)

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestInsert10, InsertItem10)

// If this flakes, disable and log details in http://crbug.com/523255.
#if defined(MEMORY_SANITIZER)
#define MAYBE_InsertItem12 DISABLED_InsertItem12
#else
#define MAYBE_InsertItem12 InsertItem12
#endif
VIEW_TEST(MenuItemViewTestInsert12, MAYBE_InsertItem12)

// If this flakes, disable and log details in http://crbug.com/523255.
#if defined(MEMORY_SANITIZER)
#define MAYBE_InsertItem20 DISABLED_InsertItem20
#else
#define MAYBE_InsertItem20 InsertItem20
#endif
VIEW_TEST(MenuItemViewTestInsert20, MAYBE_InsertItem20)

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestInsert22, InsertItem22)

// Test class for inserting a menu item while a submenu is open.
template <int INSERT_INDEX>
class MenuItemViewTestInsertWithSubmenu : public MenuTestBase {
 public:
  MenuItemViewTestInsertWithSubmenu() = default;
  MenuItemViewTestInsertWithSubmenu(const MenuItemViewTestInsertWithSubmenu&) =
      delete;
  MenuItemViewTestInsertWithSubmenu& operator=(
      const MenuItemViewTestInsertWithSubmenu&) = delete;
  ~MenuItemViewTestInsertWithSubmenu() override = default;

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    submenu_ = menu->AppendSubMenu(1, u"My Submenu");
    submenu_->AppendMenuItem(101, u"submenu item 1");
    submenu_->AppendMenuItem(101, u"submenu item 2");
    menu->AppendMenuItem(2, u"item 2");
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuStart), 0);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuPopupStart), 0);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuPopupEnd), 0);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuEnd), 0);
  }

  // Post submenu.
  void DoTestWithMenuOpen() override {
    Click(submenu_,
          CreateEventTask(this, &MenuItemViewTestInsertWithSubmenu::Step2));
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuStart), 1);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuPopupStart), 1);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuPopupEnd), 0);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuEnd), 0);
  }

  // Insert item at INSERT_INDEX.
  void Step2() {
    inserted_item_ = menu()->AddMenuItemAt(
        INSERT_INDEX, 1000, u"inserted item", std::u16string(),
        std::u16string(), ui::ImageModel(), ui::ImageModel(),
        views::MenuItemView::Type::kNormal, ui::NORMAL_SEPARATOR);
    ASSERT_TRUE(inserted_item_);
    menu()->ChildrenChanged();

    Click(inserted_item_,
          CreateEventTask(this, &MenuItemViewTestInsertWithSubmenu::Step3));
  }

  void Step3() {
    Done();
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuStart), 1);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuPopupStart), 2);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuPopupEnd), 2);
    EXPECT_EQ(GetAXEventCount(ax::mojom::Event::kMenuEnd), 1);
  }

 private:
  raw_ptr<views::MenuItemView, AcrossTasksDanglingUntriaged> submenu_ = nullptr;
  raw_ptr<views::MenuItemView, AcrossTasksDanglingUntriaged> inserted_item_ =
      nullptr;
};

// MenuItemViewTestInsertWithSubmenuX posts a menu and its submenu,
// then inserts an item in the top-level menu at X.
using MenuItemViewTestInsertWithSubmenu0 = MenuItemViewTestInsertWithSubmenu<0>;
using MenuItemViewTestInsertWithSubmenu1 = MenuItemViewTestInsertWithSubmenu<1>;

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestInsertWithSubmenu0, InsertItemWithSubmenu0)

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestInsertWithSubmenu1, InsertItemWithSubmenu1)

// Test class for removing a menu item while the menu is open.
template <int REMOVE_INDEX, int SELECT_INDEX>
class MenuItemViewTestRemove : public MenuTestBase {
 public:
  MenuItemViewTestRemove() = default;
  MenuItemViewTestRemove(const MenuItemViewTestRemove&) = delete;
  MenuItemViewTestRemove& operator=(const MenuItemViewTestRemove&) = delete;
  ~MenuItemViewTestRemove() override = default;

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    menu->AppendMenuItem(1, u"item 1");
    menu->AppendMenuItem(2, u"item 2");
    menu->AppendMenuItem(3, u"item 3");
  }

  // Remove item at REMOVE_INDEX and click item at SELECT_INDEX.
  void DoTestWithMenuOpen() override {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(3u, submenu->GetMenuItems().size());

    // remove
    menu()->RemoveMenuItem(submenu->GetMenuItemAt(REMOVE_INDEX));
    menu()->ChildrenChanged();

    // click
    views::MenuItemView* item = submenu->GetMenuItemAt(SELECT_INDEX);
    ASSERT_TRUE(item);
    Click(item, CreateEventTask(this, &MenuItemViewTestRemove::Step2));
  }

  // Check clicked item and complete test.
  void Step2() {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_FALSE(submenu->IsShowing());
    ASSERT_EQ(2u, submenu->GetMenuItems().size());

    if (SELECT_INDEX < REMOVE_INDEX)
      ASSERT_EQ(SELECT_INDEX + 1, last_command());
    else
      ASSERT_EQ(SELECT_INDEX + 2, last_command());

    Done();
  }
};

using MenuItemViewTestRemove00 = MenuItemViewTestRemove<0, 0>;
using MenuItemViewTestRemove01 = MenuItemViewTestRemove<0, 1>;
using MenuItemViewTestRemove10 = MenuItemViewTestRemove<1, 0>;
using MenuItemViewTestRemove11 = MenuItemViewTestRemove<1, 1>;
using MenuItemViewTestRemove20 = MenuItemViewTestRemove<2, 0>;
using MenuItemViewTestRemove21 = MenuItemViewTestRemove<2, 1>;
// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestRemove00, RemoveItem00)

// If this flakes, disable and log details in http://crbug.com/523255.
// Super flaky on Wayland.
#if BUILDFLAG(IS_OZONE)
#define MAYBE_RemoveItem01 DISABLED_RemoveItem01
#else
#define MAYBE_RemoveItem01 RemoveItem01
#endif
VIEW_TEST(MenuItemViewTestRemove01, MAYBE_RemoveItem01)

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestRemove10, RemoveItem10)

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestRemove11, RemoveItem11)

// If this flakes, disable and log details in http://crbug.com/523255.
VIEW_TEST(MenuItemViewTestRemove20, RemoveItem20)

// If this flakes, disable and log details in http://crbug.com/523255.
// Flaky on Wayland.
#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_LINUX)
#define MAYBE_RemoveItem21 DISABLED_RemoveItem21
#else
#define MAYBE_RemoveItem21 RemoveItem21
#endif
VIEW_TEST(MenuItemViewTestRemove21, MAYBE_RemoveItem21)

// Test class for removing a menu item while a submenu is open.
template <int REMOVE_INDEX>
class MenuItemViewTestRemoveWithSubmenu : public MenuTestBase {
 public:
  MenuItemViewTestRemoveWithSubmenu() = default;
  MenuItemViewTestRemoveWithSubmenu(const MenuItemViewTestRemoveWithSubmenu&) =
      delete;
  MenuItemViewTestRemoveWithSubmenu& operator=(
      const MenuItemViewTestRemoveWithSubmenu&) = delete;
  ~MenuItemViewTestRemoveWithSubmenu() override = default;

  // MenuTestBase implementation
  void BuildMenu(views::MenuItemView* menu) override {
    menu->AppendMenuItem(1, u"item 1");
    submenu_ = menu->AppendSubMenu(2, u"My Submenu");
    submenu_->AppendMenuItem(101, u"submenu item 1");
    submenu_->AppendMenuItem(102, u"submenu item 2");
  }

  // Post submenu.
  void DoTestWithMenuOpen() override {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());

    Click(submenu_,
          CreateEventTask(this, &MenuItemViewTestRemoveWithSubmenu::Step2));
  }

  // Remove item at REMOVE_INDEX and press escape to exit the menu loop.
  void Step2() {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_TRUE(submenu->IsShowing());
    ASSERT_EQ(2u, submenu->GetMenuItems().size());

    // remove
    menu()->RemoveMenuItem(submenu->GetMenuItemAt(REMOVE_INDEX));
    menu()->ChildrenChanged();

    // click
    KeyPress(ui::VKEY_ESCAPE,
             CreateEventTask(this, &MenuItemViewTestRemoveWithSubmenu::Step3));
  }

  void Step3() {
    views::SubmenuView* submenu = menu()->GetSubmenu();
    ASSERT_TRUE(submenu);
    ASSERT_FALSE(submenu->IsShowing());
    ASSERT_EQ(1u, submenu->GetMenuItems().size());

    Done();
  }

 private:
  raw_ptr<views::MenuItemView, AcrossTasksDanglingUntriaged> submenu_ = nullptr;
};

using MenuItemViewTestRemoveWithSubmenu0 = MenuItemViewTestRemoveWithSubmenu<0>;
using MenuItemViewTestRemoveWithSubmenu1 = MenuItemViewTestRemoveWithSubmenu<1>;

// If this flakes, disable and log details in http://crbug.com/523255.
// Flaky on Wayland.
#if BUILDFLAG(IS_OZONE)
#define MAYBE_RemoveItemWithSubmenu0 DISABLED_RemoveItemWithSubmenu0
#else
#define MAYBE_RemoveItemWithSubmenu0 RemoveItemWithSubmenu0
#endif
VIEW_TEST(MenuItemViewTestRemoveWithSubmenu0, MAYBE_RemoveItemWithSubmenu0)

// If this flakes, disable and log details in http://crbug.com/523255.
// TODO(crbug.com/40244484): Flaky on Wayland.
#if BUILDFLAG(IS_OZONE)
#define MAYBE_RemoveItemWithSubmenu1 DISABLED_RemoveItemWithSubmenu1
#else
#define MAYBE_RemoveItemWithSubmenu1 RemoveItemWithSubmenu1
#endif
VIEW_TEST(MenuItemViewTestRemoveWithSubmenu1, MAYBE_RemoveItemWithSubmenu1)
