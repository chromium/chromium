// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/retry_view.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/ui/typography.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"

namespace quick_answers {
namespace {
// TODO(b/335701090): Use LayoutProvider.
constexpr int kItemSpacing = 4;
}  // namespace

RetryView::RetryView() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetCollapseMargins(true);

  AddChildView(views::Builder<views::Label>()
                   .CopyAddressTo(&first_line_label_)
                   .SetVisible(false)
                   .SetEnabledColorId(ui::kColorLabelForeground)
                   .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                   .SetProperty(views::kFlexBehaviorKey,
                                views::FlexSpecification(
                                    views::MinimumFlexSizeRule::kScaleToZero,
                                    views::MaximumFlexSizeRule::kPreferred))
                   .SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(0, 0, kItemSpacing, 0))
                   .Build());

  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .AddChild(
              views::Builder<views::Label>()
                  .CopyAddressTo(&second_line_label_)
                  .SetEnabledColorId(ui::kColorLabelForegroundSecondary)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_QUICK_ANSWERS_VIEW_NETWORK_ERROR))
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kPreferred)))
          .AddChild(
              views::Builder<views::LabelButton>()
                  .CopyAddressTo(&retry_label_button_)
                  .SetText(
                      l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_RETRY))
                  .SetEnabledTextColorIds(ui::kColorProgressBar)
                  .SetRequestFocusOnPress(true)
                  .SetCallback(base::BindRepeating(
                      &RetryView::OnRetryButtonPressed, base::Unretained(this)))
                  .SetAccessibleName(l10n_util::GetStringFUTF16(
                      IDS_QUICK_ANSWERS_VIEW_A11Y_RETRY_LABEL_NAME_TEMPLATE,
                      l10n_util::GetStringUTF16(
                          IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT)))
                  .SetAccessibleDescription(l10n_util::GetStringUTF16(
                      IDS_QUICK_ANSWERS_VIEW_A11Y_RETRY_LABEL_DESC)))
          .Build());

  retry_label_button_->button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  SetDesign(Design::kCurrent);
}

RetryView::~RetryView() = default;

void RetryView::SetFirstLineText(const std::u16string& first_line_text) {
  first_line_label_->SetText(first_line_text);
  first_line_label_->SetVisible(!first_line_text.empty());
}

std::u16string RetryView::GetFirstLineText() const {
  return first_line_label_->GetText();
}

void RetryView::SetRetryButtonCallback(
    RetryButtonCallback retry_button_callback) {
  retry_button_callback_ = retry_button_callback;
}

void RetryView::SetDesign(Design design) {
  first_line_label_->SetFontList(GetFirstLineFontList(design));
  first_line_label_->SetLineHeight(GetFirstLineHeight(design));

  second_line_label_->SetFontList(GetSecondLineFontList(design));
  second_line_label_->SetLineHeight(GetSecondLineHeight(design));
}

void RetryView::OnRetryButtonPressed() {
  if (retry_button_callback_.is_null()) {
    return;
  }

  retry_button_callback_.Run();
}

BEGIN_METADATA(RetryView)
ADD_PROPERTY_METADATA(std::u16string, FirstLineText)
END_METADATA

}  // namespace quick_answers
