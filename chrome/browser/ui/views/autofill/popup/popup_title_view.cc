// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_title_view.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

PopupTitleView::PopupTitleView(std::u16string_view title) {
  SetUseDefaultFillLayout(true);
  SetFocusBehavior(FocusBehavior::NEVER);
  SetProperty(views::kMarginsKey,
              gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_CONTENT_LIST_VERTICAL_MULTI),
                              PopupBaseView::ArrowHorizontalMargin()));
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));
  AddChildView(views::Builder<views::Label>()
                   .SetText(base::ToUpperASCII(title))
                   .SetEnabledColorId(ui::kColorLabelForegroundSecondary)
                   .SetTextStyle(views::style::STYLE_CAPTION_BOLD)
                   .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                   .Build());

  GetViewAccessibility().SetRole(ax::mojom::Role::kLabelText);
}

PopupTitleView::~PopupTitleView() = default;

BEGIN_METADATA(PopupTitleView)
END_METADATA

}  // namespace autofill
