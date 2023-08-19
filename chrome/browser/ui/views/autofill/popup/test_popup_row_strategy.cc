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

void TestAccessibilityDelegate::GetAccessibleNodeData(
    bool is_selected,
    ui::AXNodeData* node_data) const {
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->SetNameChecked(kVoiceOverName);
}

TestPopupRowStrategy::TestPopupRowStrategy(int line_number, bool has_control)
    : line_number_(line_number), has_control_(has_control) {}

TestPopupRowStrategy::~TestPopupRowStrategy() = default;

std::unique_ptr<PopupCellView> TestPopupRowStrategy::CreateContent() {
  std::unique_ptr<PopupCellView> cell =
      views::Builder<PopupCellView>(std::make_unique<PopupCellView>())
          .SetAccessibilityDelegate(
              std::make_unique<TestAccessibilityDelegate>())
          .SetUseDefaultFillLayout(true)
          .Build();
  cell->AddChildView(std::make_unique<views::Label>(u"Test content"));
  return cell;
}

std::unique_ptr<PopupCellView> TestPopupRowStrategy::CreateControl() {
  if (!has_control_) {
    return nullptr;
  }
  std::unique_ptr<PopupCellView> cell =
      views::Builder<PopupCellView>(std::make_unique<PopupCellView>())
          .SetAccessibilityDelegate(
              std::make_unique<TestAccessibilityDelegate>())
          .SetUseDefaultFillLayout(true)
          .Build();
  cell->AddChildView(std::make_unique<views::Label>(u"Test control"));
  return cell;
}

int TestPopupRowStrategy::GetLineNumber() const {
  return line_number_;
}

}  // namespace autofill
