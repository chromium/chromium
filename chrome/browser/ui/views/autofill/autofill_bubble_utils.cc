// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

constexpr int kIconSize = 16;

}  // namespace

std::unique_ptr<views::ImageButton> CreateEditButton(
    views::Button::PressedCallback callback) {
  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(
          std::move(callback),
          ::features::IsRoundedIconsEnabled() ? vector_icons::kEditFilledIcon
                                              : vector_icons::kEditOldIcon,
          kIconSize);
  button->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
  InstallCircleHighlightPathGenerator(button.get());
  return button;
}

std::unique_ptr<views::View> CreateLegalMessageView(
    const LegalMessageLines& legal_message_lines,
    std::optional<int> text_style,
    std::optional<ui::ColorId> text_color_id,
    std::optional<int> link_text_style,
    base::RepeatingCallback<void(const GURL&)> callback) {
  auto result = views::Builder<views::BoxLayoutView>()
                    .SetOrientation(views::BoxLayout::Orientation::kVertical)
                    .SetBetweenChildSpacing(
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_RELATED_CONTROL_VERTICAL_SMALL))
                    .Build();
  for (const LegalMessageLine& line : legal_message_lines) {
    auto label = views::Builder<views::StyledLabel>()
                     .SetText(line.text())
                     .SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL)
                     .SetDefaultTextStyle(
                         text_style.value_or(views::style::STYLE_SECONDARY))
                     .SetDefaultEnabledColorId(text_color_id)
                     .Build();
    for (const LegalMessageLine::Link& link : line.links()) {
      views::StyledLabel::RangeStyleInfo link_style =
          views::StyledLabel::RangeStyleInfo::CreateForLink(
              base::BindRepeating(callback, link.url));
      link_style.text_style =
          link_text_style.value_or(views::style::STYLE_LINK);
      label->AddStyleRange(link.range, link_style);
    }
    result->AddChildView(std::move(label));
  }
  return result;
}

}  // namespace autofill
