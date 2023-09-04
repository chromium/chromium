// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_TEST_POPUP_ROW_STRATEGY_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_TEST_POPUP_ROW_STRATEGY_H_

#include <memory>

#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"

namespace autofill {

class PopupCellView;

class TestAccessibilityDelegate : public PopupCellView::AccessibilityDelegate {
 public:
  static constexpr char16_t kVoiceOverName[] = u"Sample voice over name";

  TestAccessibilityDelegate() = default;
  ~TestAccessibilityDelegate() override = default;

  void GetAccessibleNodeData(bool is_selected,
                             bool is_permanently_highlighted,
                             ui::AXNodeData* node_data) const override;
};

// A `PopupRowStrategy` used solely in tests.
class TestPopupRowStrategy : public PopupRowStrategy {
 public:
  TestPopupRowStrategy(int line_number, bool has_control);
  ~TestPopupRowStrategy() override;

  std::unique_ptr<PopupCellView> CreateContent() override;
  // Creates the control view. Returns `nullptr` if `has_control_` is false.
  std::unique_ptr<PopupCellView> CreateControl() override;

  int GetLineNumber() const override;

 private:
  const int line_number_;
  const bool has_control_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_TEST_POPUP_ROW_STRATEGY_H_
