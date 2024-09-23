// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
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

// A widget that always claims to be active, regardless of its real activation
// status. We need this in our tests to make sure that `FocusManager` always
// request focus on a view regardless of its widget activation state. Note that
// we need this because you cannot activate a widget in a test unless it's a
// part of interactive_ui_tests
class ActiveWidget : public views::Widget {
 public:
  ActiveWidget() = default;

  ActiveWidget(const ActiveWidget&) = delete;
  ActiveWidget& operator=(const ActiveWidget&) = delete;

  ~ActiveWidget() override = default;

  // views::Widget:
  bool IsActive() const override { return true; }
};

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

std::unique_ptr<views::View> CreateFocusableView() {
  auto view = std::make_unique<views::View>();
  // Set up view so that it is focusable during test.
  view->SetEnabled(true);
  view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  return view;
}

constexpr int kTraversableViewsNumber = 5;

class TestHandlerDelegate : public PreTargetHandler::Delegate {
 public:
  explicit TestHandlerDelegate(views::View* root_view) : root_view_(root_view) {
    for (int i = 0; i < kTraversableViewsNumber; i++) {
      auto* view = root_view_->AddChildView(CreateFocusableView());
      traversable_views_.emplace_back(view);
    }
  }
  TestHandlerDelegate(const TestHandlerDelegate&) = delete;
  TestHandlerDelegate& operator=(const TestHandlerDelegate&) = delete;
  ~TestHandlerDelegate() = default;

  // PreTargetHandler::Delegate:
  views::View* GetRootView() override { return root_view_; }

  std::vector<views::View*> GetTraversableViewsByUpDownKeys() override {
    return traversable_views_;
  }

  views::View* GetTraversableViewByIndex(int index) {
    return traversable_views_[index];
  }

 private:
  const raw_ptr<views::View> root_view_;
  std::vector<views::View*> traversable_views_;
};

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

    test_widget_ = std::make_unique<ActiveWidget>();
    test_widget_->Init(CreateParamsForTestWidget());

    auto contents_view = std::make_unique<views::BoxLayoutView>();
    test_view_ = contents_view->AddChildView(CreateFocusableView());

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
  raw_ptr<views::View> test_view_;
  std::unique_ptr<TestMenuModelDelegate> menu_delegate_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<ActiveWidget> test_widget_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PreTargetHandlerTest,
                         testing::Values(CardType::kDefault,
                                         CardType::kEditorMenu,
                                         CardType::kMahiDefaultMenu,
                                         CardType::kMagicBoostOptInCard));

TEST_P(PreTargetHandlerTest, KeyUpWhenNoItemSelected) {
  auto card_type = GetCardType();
  TestHandlerDelegate delegate(test_view_);
  PreTargetHandler handler(delegate, card_type);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));

  EXPECT_EQ(GetContextMenuSelectedState(),
            ContextMenuSelectedState::kNoItemSelected);

  event_generator.PressAndReleaseKey(ui::VKEY_UP);

  // When no item is selected in the menu, the first traversable view should
  // request focus when key up is hit if it is a `kDefault` card. Otherwise the
  // view should not request focus
  EXPECT_EQ(card_type == CardType::kDefault,
            delegate.GetTraversableViewByIndex(0)->HasFocus());
}

TEST_P(PreTargetHandlerTest, FirstItemSelected) {
  auto card_type = GetCardType();
  TestHandlerDelegate delegate(test_view_);
  PreTargetHandler handler(delegate, card_type);

  // Hitting down should select the first menu item.
  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);

  ASSERT_EQ(GetContextMenuSelectedState(),
            ContextMenuSelectedState::kFirstItemSelected);

  // Going up. The last traversable view should be focus if it is a `kDefault`
  // card. Otherwise the focus should move down to the last menu item.
  event_generator.PressAndReleaseKey(ui::VKEY_UP);

  EXPECT_EQ(card_type == CardType::kDefault,
            delegate.GetTraversableViewByIndex(kTraversableViewsNumber - 1)
                ->HasFocus());
  EXPECT_EQ(card_type != CardType::kDefault,
            GetContextMenuSelectedState() ==
                ContextMenuSelectedState::kLastItemSelected);
}

TEST_P(PreTargetHandlerTest, LastItemSelected) {
  auto card_type = GetCardType();
  TestHandlerDelegate delegate(test_view_);
  PreTargetHandler handler(delegate, card_type);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);

  ASSERT_EQ(GetContextMenuSelectedState(),
            ContextMenuSelectedState::kLastItemSelected);

  // At the last menu item, going down should focus the first traversable view
  // if it is a `kDefault` card. Otherwise the focus should move up to the first
  // menu item.
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);

  EXPECT_EQ(card_type == CardType::kDefault,
            delegate.GetTraversableViewByIndex(0)->HasFocus());
  EXPECT_EQ(card_type != CardType::kDefault,
            GetContextMenuSelectedState() ==
                ContextMenuSelectedState::kFirstItemSelected);
}

TEST_P(PreTargetHandlerTest, ViewFocusedKeyDown) {
  auto card_type = GetCardType();
  if (card_type != CardType::kDefault) {
    GTEST_SKIP() << "This test only applies to kDefault type";
  }

  TestHandlerDelegate delegate(test_view_);
  PreTargetHandler handler(delegate, CardType::kDefault);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));
  event_generator.PressAndReleaseKey(ui::VKEY_UP);

  // Traversing to the last view.
  for (int i = 0; i < kTraversableViewsNumber - 1; i++) {
    ASSERT_TRUE(delegate.GetTraversableViewByIndex(i)->HasFocus())
        << "should focus view at index " << i;
    event_generator.PressAndReleaseKey(ui::VKEY_DOWN);
  }

  // When the last traversable view is focused, going down should take focus to
  // the first menu item.
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);

  EXPECT_EQ(GetContextMenuSelectedState(),
            ContextMenuSelectedState::kFirstItemSelected);

  // Going up will take focus back to the last traversable view.
  event_generator.PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_TRUE(delegate.GetTraversableViewByIndex(kTraversableViewsNumber - 1)
                  ->HasFocus());
}

TEST_P(PreTargetHandlerTest, ViewFocusedKeyUp) {
  auto card_type = GetCardType();
  if (card_type != CardType::kDefault) {
    GTEST_SKIP() << "This test only applies to kDefault type";
  }

  TestHandlerDelegate delegate(test_view_);
  PreTargetHandler handler(delegate, CardType::kDefault);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));
  event_generator.PressAndReleaseKey(ui::VKEY_UP);

  ASSERT_TRUE(delegate.GetTraversableViewByIndex(0)->HasFocus());

  // When the first traversable is focused, going up should take focus to the
  // last menu item.
  event_generator.PressAndReleaseKey(ui::VKEY_UP);

  EXPECT_EQ(GetContextMenuSelectedState(),
            ContextMenuSelectedState::kLastItemSelected);

  // Going down will take focus back to the first focusable view.
  event_generator.PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(delegate.GetTraversableViewByIndex(0)->HasFocus());
}

TEST_P(PreTargetHandlerTest, TraverseBetweenViews) {
  auto card_type = GetCardType();
  if (card_type != CardType::kDefault) {
    GTEST_SKIP() << "This test only applies to kDefault type";
  }

  TestHandlerDelegate delegate(test_view_);
  PreTargetHandler handler(delegate, CardType::kDefault);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));
  event_generator.PressAndReleaseKey(ui::VKEY_UP);

  // Traversing down the list using VKEY_DOWN.
  for (int i = 0; i < kTraversableViewsNumber - 1; i++) {
    EXPECT_TRUE(delegate.GetTraversableViewByIndex(i)->HasFocus())
        << "should focus view at index " << i;
    event_generator.PressAndReleaseKey(ui::VKEY_DOWN);
  }

  // Traversing up the list using VKEY_UP.
  for (int i = kTraversableViewsNumber - 1; i >= 0; i--) {
    EXPECT_TRUE(delegate.GetTraversableViewByIndex(i)->HasFocus())
        << "should focus view at index " << i;
    event_generator.PressAndReleaseKey(ui::VKEY_UP);
  }
}

TEST_P(PreTargetHandlerTest, NavigateUsingTabKey) {
  auto card_type = GetCardType();

  TestHandlerDelegate delegate(test_view_);
  PreTargetHandler handler(delegate, card_type);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(test_widget_.get()));
  event_generator.PressAndReleaseKey(ui::VKEY_TAB);

  // All the card types besides `kDefault` should be focusable using tab keys.
  EXPECT_EQ(card_type != CardType::kDefault, test_view_->HasFocus());
}

}  // namespace

}  // namespace chromeos::editor_menu
