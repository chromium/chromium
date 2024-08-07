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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"

namespace autofill {

PopupSeparatorView::PopupSeparatorView(int vertical_padding) {
  SetFocusBehavior(FocusBehavior::NEVER);
  SetUseDefaultFillLayout(true);
  AddChildView(views::Builder<views::Separator>()
                   .SetBorder(views::CreateEmptyBorder(
                       gfx::Insets::VH(vertical_padding, 0)))
                   .SetColorId(ui::kColorSeparator)
                   .Build());
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));

  GetViewAccessibility().SetRole(ax::mojom::Role::kSplitter);
}

PopupSeparatorView::~PopupSeparatorView() = default;

BEGIN_METADATA(PopupSeparatorView)
END_METADATA

}  // namespace autofill
