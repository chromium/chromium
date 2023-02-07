// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_warning_view.h"

#include <memory>

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace autofill {

PopupWarningView::PopupWarningView(const Suggestion& suggestion)
    : text_value_(suggestion.main_text.value) {
  SetUseDefaultFillLayout(true);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
      PopupBaseView::GetCornerRadius(), PopupBaseView::GetHorizontalMargin())));

  AddChildView(views::Builder<views::Label>()
                   .SetText(text_value_)
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetTextStyle(ChromeTextStyle::STYLE_RED)
                   .SetMultiLine(true)
                   .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                   .SetEnabledColorId(ui::kColorAlertHighSeverity)
                   .Build());
}

PopupWarningView::~PopupWarningView() = default;

void PopupWarningView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kStaticText;
  node_data->SetNameChecked(text_value_);
}

BEGIN_METADATA(PopupWarningView, views::View)
END_METADATA

}  // namespace autofill
