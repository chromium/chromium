// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_MOCK_SELECTION_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_MOCK_SELECTION_DELEGATE_H_

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
class MockSelectionDelegate : public PopupRowView::SelectionDelegate {
 public:
  MockSelectionDelegate();
  ~MockSelectionDelegate() override;

  MOCK_METHOD(std::optional<CellIndex>, GetSelectedCell, (), (const override));
  MOCK_METHOD(void,
              SetSelectedCell,
              (std::optional<CellIndex>, PopupCellSelectionSource),
              (override));
};
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_MOCK_SELECTION_DELEGATE_H_
