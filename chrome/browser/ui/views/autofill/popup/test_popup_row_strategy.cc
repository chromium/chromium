// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/test_popup_row_strategy.h"

#include <memory>

#include "components/autofill/core/common/aliases.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/controls/label.h"

namespace autofill {

TestPopupRowStrategy::TestPopupRowStrategy(int line_number)
    : line_number_(line_number) {}

TestPopupRowStrategy::~TestPopupRowStrategy() = default;

std::unique_ptr<PopupRowContentView> TestPopupRowStrategy::CreateContent() {
  auto cell = std::make_unique<PopupRowContentView>();
  cell->SetUseDefaultFillLayout(true);
  cell->AddChildView(std::make_unique<views::Label>(u"Test content"));
  return cell;
}

int TestPopupRowStrategy::GetLineNumber() const {
  return line_number_;
}

}  // namespace autofill
