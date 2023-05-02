// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_resource_view.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/controls/label.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    HighEfficiencyResourceView,
    kHighEfficiencyResourceViewMemorySavingsElementId);

HighEfficiencyResourceView::HighEfficiencyResourceView(
    const int memory_savings_bytes) {
  SetOrientation(views::LayoutOrientation::kVertical);

  auto* memory_label = AddChildView(
      std::make_unique<views::Label>(ui::FormatBytes(memory_savings_bytes)));
  memory_label->SetProperty(views::kElementIdentifierKey,
                            kHighEfficiencyResourceViewMemorySavingsElementId);
  AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_SAVINGS_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
}
