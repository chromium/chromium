// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_
#define CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_

#include "chrome/test/base/chrome_ash_test_base.h"

namespace ash {
class QuickAnswersController;
}  // namespace ash

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

 private:
  std::unique_ptr<ash::QuickAnswersController> quick_answers_controller_;
};

#endif  // CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_TEST_CHROME_QUICK_ANSWERS_TEST_BASE_H_
