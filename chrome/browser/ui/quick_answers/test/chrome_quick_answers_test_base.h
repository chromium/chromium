// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_

#include <memory>

#include "chrome/test/base/chrome_ash_test_base.h"

class QuickAnswersController;
class TestingProfile;

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
  void CreateAndShowBasicMenu();
  void ResetMenuParent();

 private:
  // Menu.
  std::unique_ptr<views::Label> menu_delegate_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<views::Widget> menu_parent_;

  std::unique_ptr<QuickAnswersController> quick_answers_controller_;
  std::unique_ptr<TestingProfile> profile_;
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_
