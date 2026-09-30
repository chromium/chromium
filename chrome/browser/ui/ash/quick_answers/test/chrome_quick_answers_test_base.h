// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_
#define CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/session_manager/test/user_session_test_environment.h"

class QuickAnswersController;

namespace user_manager {
class User;
}  // namespace user_manager

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class Label;
class MenuRunner;
class Widget;
}  // namespace views

// Helper class for Quick Answers related tests.
class ChromeQuickAnswersTestBase : public ChromeAshTestBase {
 public:
  ChromeQuickAnswersTestBase();

  ChromeQuickAnswersTestBase(const ChromeQuickAnswersTestBase&) = delete;
  ChromeQuickAnswersTestBase& operator=(const ChromeQuickAnswersTestBase&) =
      delete;

  ~ChromeQuickAnswersTestBase() override;

  // ChromeAshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  // `SetUpInitialPrefValues` is called before `QuickAnswersState` is
  // instantiated.
  virtual void SetUpInitialPrefValues() {}

  // Start the new user session for UserManager and SessionManager.
  // Note: SessionController etc. are not following this, because
  // they need to be set up after Profile creation.
  virtual user_manager::User* StartUserSession();

  // A test can override this method to inject `FakeQuickAnswersState` into
  // `QuickAnswersControllerImpl`.
  virtual std::unique_ptr<QuickAnswersControllerImpl>
  CreateQuickAnswersControllerImpl(
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller);

  void CreateAndShowBasicMenu();
  void ResetMenuParent();
  TestingProfile* GetProfile() { return profile_; }
  ash::test::UserSessionTestEnvironment* user_session_test_environment() {
    return user_session_test_environment_.get();
  }

 private:
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<TestingProfile> profile_ = nullptr;

  // Menu.
  std::unique_ptr<views::Label> menu_delegate_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<views::Widget> menu_parent_;

  chromeos::ReadWriteCardsUiController read_write_cards_ui_controller_;
  std::unique_ptr<QuickAnswersController> quick_answers_controller_;
};

#endif  // CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_
