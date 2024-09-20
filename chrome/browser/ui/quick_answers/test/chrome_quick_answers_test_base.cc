// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

ChromeQuickAnswersTestBase::ChromeQuickAnswersTestBase() = default;

ChromeQuickAnswersTestBase::~ChromeQuickAnswersTestBase() = default;

ash::FakeChromeUserManager* GetFakeUserManager() {
  return static_cast<ash::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
}

void ChromeQuickAnswersTestBase::SetUp() {
  ChromeAshTestBase::SetUp();

  TestingProfile::Builder profile_builder;
  profile_ = profile_builder.Build();
  auto account_id = AccountId::FromUserEmail(profile_->GetProfileUserName());
  GetFakeUserManager()->AddUser(account_id);
  GetFakeUserManager()->LoginUser(account_id);

  SetUpInitialPrefValues();
  quick_answers_controller_ =
      CreateQuickAnswersControllerImpl(read_write_cards_ui_controller_);

  CreateUserSessions(/*session_count=*/1);
}

void ChromeQuickAnswersTestBase::TearDown() {
  quick_answers_controller_.reset();

  // Menu.
  menu_parent_.reset();
  menu_runner_.reset();
  menu_model_.reset();
  menu_delegate_.reset();

  ChromeAshTestBase::TearDown();
}

std::unique_ptr<QuickAnswersControllerImpl>
ChromeQuickAnswersTestBase::CreateQuickAnswersControllerImpl(
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller) {
  return std::make_unique<QuickAnswersControllerImpl>(
      read_write_cards_ui_controller);
}

void ChromeQuickAnswersTestBase::CreateAndShowBasicMenu() {
  menu_delegate_ = std::make_unique<views::Label>();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(menu_delegate_.get());
  menu_model_->AddItem(0, u"Menu item");
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_parent_ =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  menu_runner_->RunMenuAt(menu_parent_.get(), nullptr, gfx::Rect(),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::MENU_SOURCE_MOUSE);
}

void ChromeQuickAnswersTestBase::ResetMenuParent() {
  CHECK(menu_parent_.get() != nullptr);
  menu_parent_.reset();
}
