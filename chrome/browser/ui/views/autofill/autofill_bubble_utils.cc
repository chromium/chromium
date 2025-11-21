// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill {
namespace {

constexpr int kIconSize = 16;
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
          .SetText(attribute_name)
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
    bool use_medium_font) {
  std::unique_ptr<views::View> attribute_value_wrapper =
      CreateAutofillAiBubbleAttributeCellContainer(
          views::BoxLayout::CrossAxisAlignment::kEnd);
  std::unique_ptr<views::Label> label =
      views::Builder<views::Label>()
          .SetText(attribute_value)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
          .SetTextStyle(use_medium_font ? views::style::STYLE_BODY_4_MEDIUM
                                        : views::style::STYLE_BODY_4)
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
          .SetTextStyle(views::style::STYLE_BODY_4_MEDIUM)
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
    label->SetText(attribute_value);
    updated_entity_dot_and_value_wrapper->AddChildView(std::move(label));
    return attribute_value_wrapper;
  }
  const std::u16string& first_line = substrings[0];
  label->SetText(first_line);

  updated_entity_dot_and_value_wrapper->AddChildView(std::move(label));
  // One line was not enough.
  if (first_line != attribute_value) {
    std::u16string remaining_lines = attribute_value.substr(first_line.size());
    base::TrimWhitespace(std::move(remaining_lines), base::TRIM_ALL,
                         &remaining_lines);
    attribute_value_wrapper->AddChildView(
        views::Builder<views::Label>()
            .SetText(remaining_lines)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
            .SetTextStyle(use_medium_font ? views::style::STYLE_BODY_4_MEDIUM
                                          : views::style::STYLE_BODY_4)
            .SetAccessibleRole(ax::mojom::Role::kDefinition)
            .SetMultiLine(true)
            .SetEnabledColor(ui::kColorSysOnSurface)
            .SetAllowCharacterBreak(true)
            .SetMaximumWidth(GetAttributeCellMaxWidth())
            .Build());

    if (accessibility_value) {
      attribute_value_wrapper->SetAccessibleRole(ax::mojom::Role::kDefinition);
      attribute_value_wrapper->GetViewAccessibility().SetName(
          *accessibility_value);
    }
  }
  return attribute_value_wrapper;
}
}  // namespace

gfx::Insets GetAutofillAiBubbleInnerMargins() {
  return ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);
}

std::unique_ptr<views::ImageButton> CreateEditButton(
    views::Button::PressedCallback callback) {
  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(
          std::move(callback), vector_icons::kEditIcon, kIconSize);
  button->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
  InstallCircleHighlightPathGenerator(button.get());
  return button;
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

std::unique_ptr<views::View> CreateWalletBubbleTitleView(
    const std::u16string& title) {
  auto title_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .Build();

  auto* label = title_view->AddChildView(
      views::Builder<views::Label>()
          .SetText(title)
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
    std::u16string attribute_value,
    std::optional<std::u16string> accessibility_value,
    bool with_blue_dot,
    bool use_medium_font) {
  auto row = views::Builder<views::BoxLayoutView>()
                 .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                 .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                 .Build();

  row->AddChildView(CreateAutofillAiBubbleAttributeNameView(attribute_name));
  row->AddChildView(CreateAutofillAiBubbleAttributeValueView(
      attribute_value, accessibility_value, with_blue_dot, use_medium_font));

  // Set every child to expand with the same ratio.
  for (auto child : row->children()) {
    row->SetFlexForView(child.get(), 1);
  }
  return row;
}
}  // namespace autofill
