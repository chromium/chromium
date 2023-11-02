// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_TEST_POPUP_ROW_STRATEGY_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_TEST_POPUP_ROW_STRATEGY_H_

#include <memory>

#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"

namespace autofill {

class PopupCellView;

// A `PopupRowStrategy` used solely in tests.
class TestPopupRowStrategy : public PopupRowStrategy {
 public:
  explicit TestPopupRowStrategy(int line_number);
  ~TestPopupRowStrategy() override;

  std::unique_ptr<PopupCellView> CreateContent() override;

  int GetLineNumber() const override;

 private:
  const int line_number_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_TEST_POPUP_ROW_STRATEGY_H_
