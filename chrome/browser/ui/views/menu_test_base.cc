// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/menu_test_base.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/widget/widget.h"

MenuTestBase::MenuTestBase() : last_command_(0) {
  ax_event_counts_.fill(0);
  views::AXEventManager::Get()->AddObserver(this);
}

MenuTestBase::~MenuTestBase() {
  views::AXEventManager::Get()->RemoveObserver(this);
}

void MenuTestBase::OnViewEvent(views::View*, ax::mojom::Event event_type) {
  ++ax_event_counts_[static_cast<size_t>(event_type)];
}

int MenuTestBase::GetAXEventCount(ax::mojom::Event event_type) const {
  return ax_event_counts_[static_cast<size_t>(event_type)];
}

void MenuTestBase::Click(views::View* view, base::OnceClosure next) {
  ui_test_utils::MoveMouseToCenterAndPress(view, ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           std::move(next));
  views::test::WaitForMenuClosureAnimation();
}

void MenuTestBase::KeyPress(ui::KeyboardCode keycode, base::OnceClosure next) {
  ui_controls::SendKeyPressNotifyWhenDone(window()->GetNativeWindow(), keycode,
                                          false, false, false, false,
                                          std::move(next));
}

int MenuTestBase::GetMenuRunnerFlags() {
  return views::MenuRunner::HAS_MNEMONICS;
}

void MenuTestBase::SetUp() {
  ViewEventTestBase::SetUp();

  views::test::DisableMenuClosureAnimations();

  auto menu_owning = std::make_unique<views::MenuItemView>(/*delegate=*/this);
  menu_ = menu_owning.get();
  BuildMenu(menu_);
  menu_runner_ = std::make_unique<views::MenuRunner>(std::move(menu_owning),
                                                     GetMenuRunnerFlags());
}

void MenuTestBase::TearDown() {
  button_ = nullptr;
  menu_ = nullptr;
  // We cancel the menu first because certain operations (like a menu opened
  // with views::MenuRunner::FOR_DROP) don't take kindly to simply pulling the
  // runner out from under them.
  menu_runner_->Cancel();
  menu_runner_.reset();

  ViewEventTestBase::TearDown();
}

std::unique_ptr<views::View> MenuTestBase::CreateContentsView() {
  auto button = std::make_unique<views::MenuButton>(
      base::BindRepeating(&MenuTestBase::ButtonPressed, base::Unretained(this)),
      u"Menu Test");
  button_ = button.get();
  return button;
}

void MenuTestBase::DoTestOnMessageLoop() {
  Click(button_, CreateEventTask(this, &MenuTestBase::DoTestWithMenuOpen));
}

gfx::Size MenuTestBase::GetPreferredSizeForContents() const {
  return button_->GetPreferredSize();
}

void MenuTestBase::ButtonPressed() {
  menu_runner_->RunMenuAt(button_->GetWidget(), button_->button_controller(),
                          button_->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::MENU_SOURCE_NONE);
}

void MenuTestBase::ExecuteCommand(int id) {
  last_command_ = id;
}
