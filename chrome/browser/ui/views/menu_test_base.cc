// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/menu_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/widget/widget.h"

MenuTestBase::MenuTestBase()
    : ViewEventTestBase(), button_(nullptr), menu_(nullptr), last_command_(0) {}

MenuTestBase::~MenuTestBase() {
}

void MenuTestBase::Click(views::View* view, base::OnceClosure next) {
  ui_test_utils::MoveMouseToCenterAndPress(view, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           std::move(next));
  views::test::WaitForMenuClosureAnimation();
}

void MenuTestBase::KeyPress(ui::KeyboardCode keycode, base::OnceClosure next) {
  ui_controls::SendKeyPressNotifyWhenDone(GetWidget()->GetNativeWindow(),
                                          keycode, false, false, false, false,
                                          std::move(next));
}

int MenuTestBase::GetMenuRunnerFlags() {
  return views::MenuRunner::HAS_MNEMONICS;
}

void MenuTestBase::SetUp() {
  views::test::DisableMenuClosureAnimations();

  button_ = new views::MenuButton(base::ASCIIToUTF16("Menu Test"), this);
  menu_ = new views::MenuItemView(this);
  BuildMenu(menu_);
  menu_runner_ =
      std::make_unique<views::MenuRunner>(menu_, GetMenuRunnerFlags());

  ViewEventTestBase::SetUp();
}

void MenuTestBase::TearDown() {
  // We cancel the menu first because certain operations (like a menu opened
  // with views::MenuRunner::FOR_DROP) don't take kindly to simply pulling the
  // runner out from under them.
  menu_runner_->Cancel();

  menu_runner_.reset();
  menu_ = nullptr;
  ViewEventTestBase::TearDown();
}

views::View* MenuTestBase::CreateContentsView() {
  return button_;
}

void MenuTestBase::DoTestOnMessageLoop() {
  Click(button_, CreateEventTask(this, &MenuTestBase::DoTestWithMenuOpen));
}

gfx::Size MenuTestBase::GetPreferredSizeForContents() const {
  return button_->GetPreferredSize();
}

void MenuTestBase::ButtonPressed(views::Button* source,
                                 const ui::Event& event) {
  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  menu_runner_->RunMenuAt(source->GetWidget(), button_->button_controller(),
                          bounds, views::MenuAnchorPosition::kTopLeft,
                          ui::MENU_SOURCE_NONE);
}

void MenuTestBase::ExecuteCommand(int id) {
  last_command_ = id;
}
