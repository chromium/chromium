// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/loading_view.h"

#include "chrome/browser/ui/quick_answers/ui/typography.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_utils.h"

namespace quick_answers {
namespace {
// TODO(b/335701090): Use LayoutProvider.
constexpr int kItemSpacing = 4;
}

LoadingView::LoadingView() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetCollapseMargins(true);

  first_line_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetEnabledColorId(ui::kColorLabelForeground)
          // Default is `ALIGN_CENTER`. See `Label::Init`.
          // `SetHorizontalAlignment` flips the value for RTL.
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kPreferred))
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(0, 0, kItemSpacing, 0))
          .Build());

  second_line_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_LOADING))
          .SetEnabledColorId(ui::kColorLabelForegroundSecondary)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kPreferred))
          .Build());

  SetDesign(Design::kCurrent);
}

void LoadingView::SetFirstLineText(const std::u16string& first_line_text) {
  first_line_label_->SetText(first_line_text);
}

std::u16string LoadingView::GetFirstLineText() const {
  return first_line_label_->GetText();
}

void LoadingView::SetDesign(Design design) {
  first_line_label_->SetFontList(GetFirstLineFontList(design));
  first_line_label_->SetLineHeight(GetFirstLineHeight(design));

  second_line_label_->SetFontList(GetSecondLineFontList(design));
  second_line_label_->SetLineHeight(GetSecondLineHeight(design));
}

BEGIN_METADATA(LoadingView)
ADD_PROPERTY_METADATA(std::u16string, FirstLineText)
END_METADATA

}  // namespace quick_answers
