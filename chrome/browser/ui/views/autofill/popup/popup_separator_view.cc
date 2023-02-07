// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"

namespace autofill {

PopupSeparatorView::PopupSeparatorView() {
  SetFocusBehavior(FocusBehavior::NEVER);
  SetUseDefaultFillLayout(true);
  const int kVerticalPadding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
  AddChildView(views::Builder<views::Separator>()
                   .SetBorder(views::CreateEmptyBorder(
                       gfx::Insets::VH(kVerticalPadding, 0)))
                   .SetColorId(ui::kColorSeparator)
                   .Build());
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));
}

PopupSeparatorView::~PopupSeparatorView() = default;

void PopupSeparatorView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Separators are not selectable.
  node_data->role = ax::mojom::Role::kSplitter;
}

BEGIN_METADATA(PopupSeparatorView, views::View)
END_METADATA

}  // namespace autofill
