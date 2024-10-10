// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_BROWSERTEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"

namespace quick_answers {
class QuickAnswersBrowserTestBase : public InProcessBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  struct ShowMenuParams {
    std::string selected_text;
    int x = 0;
    int y = 0;
    bool is_password_field = false;
  };

  QuickAnswersBrowserTestBase();
  ~QuickAnswersBrowserTestBase() override;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

  bool IsMagicBoostEnabled() const;

 protected:
  // `ShowMenu` generates a web page with `params.selected_text` at a position
  // of (`params.x`, `params.y`) and right click on it.
  void ShowMenu(const ShowMenuParams& params);

  // Show a context menu and wait until it's shown. Note that this only waits
  // context menu. Quick answers might require additional async operations
  // before it's shown.
  void ShowMenuAndWait(const ShowMenuParams& params);

  QuickAnswersController* controller() { return QuickAnswersController::Get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_BROWSERTEST_BASE_H_
