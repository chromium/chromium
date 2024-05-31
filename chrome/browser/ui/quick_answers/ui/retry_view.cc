// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/retry_view.h"

#include <string>

#include "base/functional/bind.h"
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
  SetDefault(views::kMarginsKey, gfx::Insets::VH(kItemSpacing, 0));
  SetCollapseMargins(true);

  views::Label* first_line_label;
  AddChildView(views::Builder<views::Label>()
                   .CopyAddressTo(&first_line_label)
                   .SetVisible(false)
                   .SetEnabledColorId(ui::kColorLabelForeground)
                   .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                   .SetProperty(views::kFlexBehaviorKey,
                                views::FlexSpecification(
                                    views::MinimumFlexSizeRule::kScaleToZero,
                                    views::MaximumFlexSizeRule::kPreferred))
                   .Build());

  CHECK(first_line_label);
  first_line_label_ = first_line_label;

  views::LabelButton* retry_label_button;
  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .AddChild(
              views::Builder<views::Label>()
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
                  .CopyAddressTo(&retry_label_button)
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

  CHECK(retry_label_button);
  retry_label_button->button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  retry_label_button_ = retry_label_button;
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
