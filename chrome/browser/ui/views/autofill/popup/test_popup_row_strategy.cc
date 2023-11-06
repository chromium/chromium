// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/test_popup_row_strategy.h"

#include <memory>

#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "components/autofill/core/common/aliases.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/controls/label.h"

namespace autofill {

TestPopupRowStrategy::TestPopupRowStrategy(int line_number)
    : line_number_(line_number) {}

TestPopupRowStrategy::~TestPopupRowStrategy() = default;

std::unique_ptr<PopupCellView> TestPopupRowStrategy::CreateContent() {
  std::unique_ptr<PopupCellView> cell =
      views::Builder<PopupCellView>(std::make_unique<PopupCellView>())
          .SetUseDefaultFillLayout(true)
          .Build();
  cell->AddChildView(std::make_unique<views::Label>(u"Test content"));
  return cell;
}

int TestPopupRowStrategy::GetLineNumber() const {
  return line_number_;
}

}  // namespace autofill
