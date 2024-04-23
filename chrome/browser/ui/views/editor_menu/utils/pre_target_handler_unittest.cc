// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace chromeos::editor_menu {

namespace {

class TestMenuModelDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  TestMenuModelDelegate() = default;
  TestMenuModelDelegate(const TestMenuModelDelegate&) = delete;
  TestMenuModelDelegate& operator=(const TestMenuModelDelegate&) = delete;
  ~TestMenuModelDelegate() override = default;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {}
};

enum class ContextMenuSelectedState {
  kNoItemSelected = 0,
  kFirstItemSelected = 1,
  kLastItemSelected = 2,
  kOther = 3,
  kMaxValue = kOther,
};

ContextMenuSelectedState GetContextMenuSelectedState() {
  auto* active_menu = views::MenuController::GetActiveInstance();
  CHECK(active_menu);

  auto* const selected_item = active_menu->GetSelectedMenuItem();
  if (!selected_item) {
    return ContextMenuSelectedState::kNoItemSelected;
  }

  auto* const parent = selected_item->GetParentMenuItem();
  if (!parent) {
    // Selected menu-item will have no parent only when there are no nested
    // menus and no items are visibly selected.
    return ContextMenuSelectedState::kNoItemSelected;
  }

  if (selected_item == parent->GetSubmenu()->children().front()) {
    return ContextMenuSelectedState::kFirstItemSelected;
  } else if (selected_item == parent->GetSubmenu()->children().back()) {
    return ContextMenuSelectedState::kLastItemSelected;
  } else {
    return ContextMenuSelectedState::kOther;
  }
}

class TestView : public views::View {
  METADATA_HEADER(TestView, views::View)

 public:
  TestView() = default;
  TestView(const TestView&) = delete;
  TestView& operator=(const TestView&) = delete;
  ~TestView() override = default;

  // views::View:
  void RequestFocus() override {
    requested_focus_ = true;
    views::View::RequestFocus();
  }

  bool requested_focus() { return requested_focus_; }

 private:
  bool requested_focus_ = false;
};

BEGIN_METADATA(TestView)
END_METADATA

class PreTargetHandlerTest : public ChromeViewsTestBase,
                             public testing::WithParamInterface<CardType> {
 public:
  PreTargetHandlerTest() = default;
  PreTargetHandlerTest(const PreTargetHandlerTest&) = delete;
  PreTargetHandlerTest& operator=(const PreTargetHandlerTest&) = delete;
  ~PreTargetHandlerTest() override = default;

  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    test_widget_ = CreateTestWidget();

    auto contents_view = std::make_unique<views::BoxLayoutView>();
    test_view_ = contents_view->AddChildView(std::make_unique<TestView>());
    test_widget_->SetContentsView(std::move(contents_view));

    // Create an active menu that will be used within `PreTargetHandler`.
    menu_delegate_ = std::make_unique<TestMenuModelDelegate>();
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(menu_delegate_.get());

    menu_model_->AddItem(0, u"Menu item 0");
    menu_model_->AddItem(1, u"Menu item 1");
    menu_model_->AddItem(2, u"Menu item 2");

    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
    menu_runner_->RunMenuAt(
        /*parent=*/test_widget_.get(),
        /*button_controller=*/nullptr, /*bounds=*/gfx::Rect(),
        views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);
  }

  void TearDown() override {
    test_view_ = nullptr;
    menu_runner_.reset();
    menu_model_.reset();
    menu_delegate_.reset();
    test_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  CardType GetCardType() { return GetParam(); }

 protected:
  raw_ptr<TestView> test_view_;
  std::unique_ptr<TestMenuModelDelegate> menu_delegate_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<views::Widget> test_widget_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PreTargetHandlerTest,
                         testing::Values(CardType::kDefault,
                                         CardType::kEditorMenu,
                                         CardType::kMahiDefaultMenu));

TEST_P(PreTargetHandlerTest, KeyUpWhenNoItemSelected) {
  auto card_type = GetCardType();
  PreTargetHandler handler(test_view_, card_type);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));

  EXPECT_EQ(GetContextMenuSelectedState(),
            ContextMenuSelectedState::kNoItemSelected);

  event_generator.PressAndReleaseKey(ui::VKEY_UP);

  // When no item is selected in the menu, the view should request focus when
  // key up is hit if it is a `kDefault` card. Otherwise the view should not
  // request focus
  EXPECT_EQ(card_type == CardType::kDefault, test_view_->requested_focus());
}

TEST_P(PreTargetHandlerTest, FirstItemSelected) {
  auto card_type = GetCardType();
  PreTargetHandler handler(test_view_, card_type);

  // Hitting down should select the first item.
  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);

  ASSERT_EQ(GetContextMenuSelectedState(),
            ContextMenuSelectedState::kFirstItemSelected);

  // Going up. The view should be focus if it is a `kDefault` card. Otherwise
  // the focus should move down to the last menu item.
  event_generator.PressAndReleaseKey(ui::VKEY_UP);

  EXPECT_EQ(card_type == CardType::kDefault, test_view_->requested_focus());
  EXPECT_EQ(card_type != CardType::kDefault,
            GetContextMenuSelectedState() ==
                ContextMenuSelectedState::kLastItemSelected);
}

TEST_P(PreTargetHandlerTest, LastItemSelected) {
  auto card_type = GetCardType();
  PreTargetHandler handler(test_view_, card_type);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);

  ASSERT_EQ(GetContextMenuSelectedState(),
            ContextMenuSelectedState::kLastItemSelected);

  // At the last menu item, going down should focus the view if it is a
  // `kDefault` card. Otherwise the focus should move up to the first menu item.
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);

  EXPECT_EQ(card_type == CardType::kDefault, test_view_->requested_focus());
  EXPECT_EQ(card_type != CardType::kDefault,
            GetContextMenuSelectedState() ==
                ContextMenuSelectedState::kFirstItemSelected);
}

}  // namespace

}  // namespace chromeos::editor_menu
