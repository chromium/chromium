// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_bubble_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace autofill {

namespace {

constexpr int kWalletIconSize = 20;
constexpr int kSubTitleBottomMargin = 16;
constexpr std::u16string_view kNewValueDot = u"•";

// Returns the maximum width of an attribute cell (name or value).
int GetAttributeCellMaxWidth() {
  // The maximum width is the bubble size minus its margin divided by two.
  // One half is for the entity attribute name and the other for the value.
  return (kAutofillAiBubbleWidth - GetAutofillAiBubbleInnerMargins().width() -
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL)) /
         2;
}

// Creates a container view for an attribute cell (name or value) with the
// specified cross axis alignment.
std::unique_ptr<views::View> CreateAutofillAiBubbleAttributeCellContainer(
    views::BoxLayout::CrossAxisAlignment cross_axis_alignment) {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetCrossAxisAlignment(cross_axis_alignment)
      .SetBetweenChildSpacing(0)
      .Build();
}
// Creates a view for the attribute name.
std::unique_ptr<views::View> CreateAutofillAiBubbleAttributeNameView(
    std::u16string attribute_name) {
  std::unique_ptr<views::View> attribute_name_wrapper =
      CreateAutofillAiBubbleAttributeCellContainer(
          views::BoxLayout::CrossAxisAlignment::kStart);
  attribute_name_wrapper->AddChildView(
      views::Builder<views::Label>()
          .SetText(std::move(attribute_name))
          .SetEnabledColor(ui::kColorSysOnSurfaceSubtle)
          .SetTextStyle(views::style::STYLE_BODY_4)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetAccessibleRole(ax::mojom::Role::kTerm)
          .SetElideBehavior(gfx::ELIDE_TAIL)
          .SetMaximumWidthSingleLine(GetAttributeCellMaxWidth())
          .Build());
  return attribute_name_wrapper;
}

// Creates a view for the attribute value, optionally with a blue dot and
// accessibility text.
std::unique_ptr<views::View> CreateAutofillAiBubbleAttributeValueView(
    std::u16string attribute_value,
    std::optional<std::u16string> accessibility_value,
    bool with_blue_dot,
    int new_value_font_style) {
  std::unique_ptr<views::View> attribute_value_wrapper =
      CreateAutofillAiBubbleAttributeCellContainer(
          views::BoxLayout::CrossAxisAlignment::kEnd);
  std::unique_ptr<views::Label> label =
      views::Builder<views::Label>()
          .SetText(attribute_value)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
          .SetTextStyle(new_value_font_style)
          .SetAccessibleRole(ax::mojom::Role::kDefinition)
          .SetMultiLine(true)
          .SetEnabledColor(ui::kColorSysOnSurface)
          .SetAllowCharacterBreak(true)
          .SetMaximumWidth(GetAttributeCellMaxWidth())
          .Build();

  // Only update dialogs have a dot circle in front of added or updated values.
  if (!with_blue_dot) {
    attribute_value_wrapper->AddChildView(std::move(label));
    return attribute_value_wrapper;
  }

  // In order to properly add a blue dot, it is necessary to have 3 labels.
  // 1. A blue label for the dot itself.
  // 2. A horizontally aligned label with the first line of the updated value.
  // 3. Optionally a third label with the remaining value.
  views::View* updated_entity_dot_and_value_wrapper =
      attribute_value_wrapper->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
              .Build());
  views::Label* blue_dot = updated_entity_dot_and_value_wrapper->AddChildView(
      views::Builder<views::Label>()
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
          .SetTextStyle(new_value_font_style)
          .SetEnabledColor(ui::kColorButtonBackgroundProminent)
          .SetText(base::StrCat({kNewValueDot, u" "}))
          .Build());

  // Reset the label style to handle the first line.
  label->SetMultiLine(false);
  label->SetAllowCharacterBreak(false);
  label->SetMaximumWidthSingleLine(GetAttributeCellMaxWidth() -
                                   blue_dot->GetPreferredSize().width());

  std::vector<std::u16string> substrings;
  gfx::ElideRectangleText(
      attribute_value, label->font_list(),
      GetAttributeCellMaxWidth() - blue_dot->GetPreferredSize().width(),
      label->GetLineHeight(), gfx::WRAP_LONG_WORDS, &substrings);
  // At least one string should always exist.
  if (substrings.empty()) {
    label->SetText(std::move(attribute_value));
    updated_entity_dot_and_value_wrapper->AddChildView(std::move(label));
    return attribute_value_wrapper;
  }
  const std::u16string& first_line = substrings[0];
  label->SetText(first_line);

  updated_entity_dot_and_value_wrapper->AddChildView(std::move(label));
  // One line was not enough.
  if (first_line != attribute_value) {
    auto remaining_lines = std::u16string(base::TrimWhitespace(
        std::u16string_view(attribute_value).substr(first_line.size()),
        base::TRIM_ALL));
    attribute_value_wrapper->AddChildView(
        views::Builder<views::Label>()
            .SetText(std::move(remaining_lines))
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
            .SetTextStyle(new_value_font_style)
            .SetAccessibleRole(ax::mojom::Role::kDefinition)
            .SetMultiLine(true)
            .SetEnabledColor(ui::kColorSysOnSurface)
            .SetAllowCharacterBreak(true)
            .SetMaximumWidth(GetAttributeCellMaxWidth())
            .Build());

    if (accessibility_value) {
      attribute_value_wrapper->SetAccessibleRole(ax::mojom::Role::kDefinition);
      attribute_value_wrapper->GetViewAccessibility().SetName(
          *std::move(accessibility_value));
    }
  }
  return attribute_value_wrapper;
}

// Returns an attribute row. If there was an old attribute value, it will be
// displayed with a strikethrough below the new attribute value and the new
// attribute value is bold.
std::unique_ptr<views::View> CreateRestyledAutofillAiBubbleAttributeValueView(
    std::u16string new_attribute_value,
    std::optional<std::u16string> old_attribute_value,
    std::optional<std::u16string> accessibility_value,
    int new_value_font_style) {
  std::unique_ptr<views::View> attribute_value_wrapper =
      CreateAutofillAiBubbleAttributeCellContainer(
          views::BoxLayout::CrossAxisAlignment::kEnd);

  attribute_value_wrapper->AddChildView(
      views::Builder<views::Label>()
          .SetText(std::move(new_attribute_value))
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
          .SetTextStyle(new_value_font_style)
          .SetAccessibleRole(ax::mojom::Role::kDefinition)
          .SetMultiLine(true)
          .SetEnabledColor(ui::kColorSysOnSurface)
          .SetAllowCharacterBreak(true)
          .SetMaximumWidth(GetAttributeCellMaxWidth())
          .Build());
  if (accessibility_value) {
    attribute_value_wrapper->SetAccessibleRole(ax::mojom::Role::kDefinition);
    attribute_value_wrapper->GetViewAccessibility().SetName(
        *std::move(accessibility_value));
  }

  if (old_attribute_value) {
    auto old_value_label =
        views::Builder<views::Label>()
            .SetText(*std::move(old_attribute_value))
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
            .SetTextStyle(views::style::STYLE_BODY_4)
            .SetAccessibleRole(ax::mojom::Role::kDefinition)
            .SetMultiLine(true)
            .SetEnabledColor(ui::kColorLabelForegroundDisabled)
            .SetAllowCharacterBreak(true)
            .SetMaximumWidth(GetAttributeCellMaxWidth())
            .Build();
    old_value_label->SetFontList(old_value_label->font_list().DeriveWithStyle(
        gfx::Font::STRIKE_THROUGH));
    attribute_value_wrapper->AddChildView(std::move(old_value_label));
  }
  return attribute_value_wrapper;
}

}  // namespace

gfx::Insets GetAutofillAiBubbleInnerMargins() {
  return ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);
}

ui::ImageModel CreateWalletIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return ui::ImageModel::FromVectorIcon(vector_icons::kGoogleWalletIcon,
                                        ui::kColorIcon, kWalletIconSize);

#else
  // This is a placeholder icon on non-branded builds.
  return ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon,
                                        ui::kColorIcon, kWalletIconSize);
#endif
}

std::unique_ptr<views::View> CreateWalletBubbleTitleView(std::u16string title) {
  auto title_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .Build();

  auto* label = title_view->AddChildView(
      views::Builder<views::Label>()
          .SetText(std::move(title))
          .SetTextStyle(views::style::STYLE_HEADLINE_4)
          .SetMultiLine(true)
          .SetAccessibleRole(ax::mojom::Role::kTitleBar)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .Build());

  title_view->AddChildView(
      views::Builder<views::ImageView>().SetImage(CreateWalletIcon()).Build());
  title_view->SetFlexForView(label, 1);
  return title_view;
}

std::unique_ptr<views::BoxLayoutView>
CreateAutofillAiBubbleSubtitleContainer() {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetInsideBorderInsets(gfx::Insets::TLBR(0, 0, kSubTitleBottomMargin, 0))
      .Build();
}

std::unique_ptr<views::View> CreateAutofillAiBubbleAttributeRow(
    std::u16string attribute_name,
    std::u16string new_attribute_value,
    std::optional<std::u16string> old_attribute_value,
    std::optional<std::u16string> accessibility_value,
    int new_value_font_style,
    bool with_blue_dot) {
  auto row = views::Builder<views::BoxLayoutView>()
                 .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                 .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                 .Build();

  row->AddChildView(
      CreateAutofillAiBubbleAttributeNameView(std::move(attribute_name)));
  if (base::FeatureList::IsEnabled(features::kAutofillAiNewUpdatePrompt)) {
    row->AddChildView(CreateRestyledAutofillAiBubbleAttributeValueView(
        std::move(new_attribute_value), std::move(old_attribute_value),
        std::move(accessibility_value), new_value_font_style));
  } else {
    row->AddChildView(CreateAutofillAiBubbleAttributeValueView(
        std::move(new_attribute_value), std::move(accessibility_value),
        with_blue_dot, new_value_font_style));
  }

  // Set every child to expand with the same ratio.
  for (auto child : row->children()) {
    row->SetFlexForView(child.get(), 1);
  }
  return row;
}

}  // namespace autofill
