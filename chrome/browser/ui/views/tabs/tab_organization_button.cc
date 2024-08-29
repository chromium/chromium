// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_organization_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTabOrganizeCornerRadius = 10;
constexpr int kTabOrganizeFlatCornerRadius = 4;
constexpr int kTabOrganizeLabelMargin = 10;
constexpr int kTabOrganizeCloseButtonMargin = 8;
constexpr int kTabOrganizeCloseButtonSize = 16;
}  // namespace

TabOrganizationButton::TabOrganizationButton(
    TabStripController* tab_strip_controller,
    PressedCallback pressed_callback,
    PressedCallback close_pressed_callback,
    const std::u16string& label_text,
    const std::u16string& tooltip_text,
    const std::u16string& accessibility_name,
    const ui::ElementIdentifier& element_identifier,
    Edge flat_edge)
    : TabStripControlButton(tab_strip_controller,
                            std::move(pressed_callback),
                            label_text,
                            flat_edge) {
  auto* const layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  SetProperty(views::kElementIdentifierKey, element_identifier);

  SetTooltipText(tooltip_text);
  GetViewAccessibility().SetName(accessibility_name);
  SetLabelStyle(views::style::STYLE_BODY_3_EMPHASIS);
  label()->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);

  const gfx::Insets label_margin =
      gfx::Insets().set_left(kTabOrganizeLabelMargin);
  label()->SetProperty(views::kMarginsKey, label_margin);

  SetForegroundFrameActiveColorId(kColorTabSearchButtonCRForegroundFrameActive);
  SetForegroundFrameInactiveColorId(
      kColorTabSearchButtonCRForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  set_paint_transparent_for_custom_image_theme(false);

  SetCloseButton(std::move(close_pressed_callback));

  UpdateColors();
}

TabOrganizationButton::~TabOrganizationButton() = default;

void TabOrganizationButton::SetOpacity(float factor) {
  label()->layer()->SetOpacity(factor);
  close_button_->layer()->SetOpacity(factor);
}

void TabOrganizationButton::SetWidthFactor(float factor) {
  width_factor_ = factor;
  PreferredSizeChanged();
}

gfx::Size TabOrganizationButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int full_width =
      GetLayoutManager()->GetPreferredSize(this, available_size).width();
  const int width = full_width * width_factor_;
  const int height = TabStripControlButton::CalculatePreferredSize(
                         views::SizeBounds(width, available_size.height()))
                         .height();
  return gfx::Size(width, height);
}

int TabOrganizationButton::GetCornerRadius() const {
  return kTabOrganizeCornerRadius;
}

int TabOrganizationButton::GetFlatCornerRadius() const {
  return kTabOrganizeFlatCornerRadius;
}

void TabOrganizationButton::SetCloseButton(PressedCallback pressed_callback) {
  auto close_button =
      std::make_unique<views::LabelButton>(std::move(pressed_callback));
  close_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE_CLOSE));

  const ui::ImageModel icon_image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kCloseChromeRefreshIcon,
      kColorTabSearchButtonCRForegroundFrameActive,
      kTabOrganizeCloseButtonSize);

  close_button->SetImageModel(views::Button::STATE_NORMAL, icon_image_model);
  close_button->SetImageModel(views::Button::STATE_HOVERED, icon_image_model);
  close_button->SetImageModel(views::Button::STATE_PRESSED, icon_image_model);

  close_button->SetPaintToLayer();
  close_button->layer()->SetFillsBoundsOpaquely(false);

  views::InkDrop::Get(close_button.get())
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(close_button.get())->SetHighlightOpacity(0.16f);
  views::InkDrop::Get(close_button.get())->SetVisibleOpacity(0.14f);
  views::InkDrop::Get(close_button.get())
      ->SetBaseColorId(kColorTabSearchButtonCRForegroundFrameActive);

  auto ink_drop_highlight_path =
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets());
  views::HighlightPathGenerator::Install(close_button.get(),
                                         std::move(ink_drop_highlight_path));

  close_button->SetPreferredSize(
      gfx::Size(kTabOrganizeCloseButtonSize, kTabOrganizeCloseButtonSize));
  close_button->SetBorder(nullptr);

  const gfx::Insets margin = gfx::Insets().set_left_right(
      kTabOrganizeCloseButtonMargin, kTabOrganizeCloseButtonMargin);
  close_button->SetProperty(views::kMarginsKey, margin);

  close_button_ = AddChildView(std::move(close_button));
}

BEGIN_METADATA(TabOrganizationButton)
END_METADATA
