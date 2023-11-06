// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_MOCK_ACCESSIBILITY_SELECTION_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_MOCK_ACCESSIBILITY_SELECTION_DELEGATE_H_

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
class MockAccessibilitySelectionDelegate
    : public PopupRowView::AccessibilitySelectionDelegate {
 public:
  MockAccessibilitySelectionDelegate();
  ~MockAccessibilitySelectionDelegate() override;

  MOCK_METHOD(void, NotifyAXSelection, (views::View&), (override));
};
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_MOCK_ACCESSIBILITY_SELECTION_DELEGATE_H_
