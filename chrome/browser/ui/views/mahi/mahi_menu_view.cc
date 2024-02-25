// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/views/experiment_badge.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos::mahi {

namespace {

constexpr char kWidgetName[] = "MahiMenuViewWidget";

constexpr gfx::Insets kMenuPadding = gfx::Insets::TLBR(12, 16, 12, 14);
constexpr int kButtonHeight = 16;
constexpr int kButtonCornerRadius = 8;
constexpr gfx::Insets kButtonPadding = gfx::Insets::VH(6, 8);
constexpr gfx::Insets kHeaderRowPadding = gfx::Insets::TLBR(0, 0, 12, 0);
constexpr int kHeaderRowSpacing = 8;
constexpr int kButtonTextfieldSpacing = 16;
constexpr int kButtonImageLabelSpacing = 4;
constexpr int kButtonBorderThickness = 1;

}  // namespace

MahiMenuView::MahiMenuView() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorPrimaryBackground,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kMenuRadius)));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetInteriorMargin(kMenuPadding);

  auto header_row = std::make_unique<views::FlexLayoutView>();
  header_row->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_row->SetInteriorMargin(kHeaderRowPadding);

  auto header_left_container = std::make_unique<views::FlexLayoutView>();
  header_left_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_left_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header_left_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_left_container->SetDefault(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, kHeaderRowSpacing));
  header_left_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded)));

  // TODO(b/318733118): Finish building the menu UI.
  // TODO(b/319264190): Replace the strings here with real strings.
  auto* header_label =
      header_left_container->AddChildView(std::make_unique<views::Label>(
          u"Mahi Menu", views::style::CONTEXT_DIALOG_TITLE,
          views::style::STYLE_HEADLINE_5));
  header_label->SetEnabledColorId(ui::kColorSysOnSurface);
  header_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  header_left_container->AddChildView(
      std::make_unique<chromeos::mahi::ExperimentBadge>());

  header_row->AddChildView(std::move(header_left_container));

  settings_button_ =
      header_row->AddChildView(views::ImageButton::CreateIconButton(
          views::Button::PressedCallback(), vector_icons::kSettingsOutlineIcon,
          l10n_util::GetStringUTF16(IDS_EDITOR_MENU_SETTINGS_TOOLTIP)));

  AddChildView(std::move(header_row));

  auto button = std::make_unique<views::LabelButton>(
      /*callback=*/base::BindRepeating(&MahiMenuView::OnSummaryButtonPressed,
                                       weak_ptr_factory_.GetWeakPtr()),
      /*text=*/l10n_util::GetStringUTF16(IDS_MAHI_SUMMARIZE_BUTTON_LABEL_TEXT));
  button->SetLabelStyle(views::style::STYLE_BODY_4_EMPHASIS);
  button->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(chromeos::kMahiSummarizeIcon,
                                     ui::kColorSysOnSurface, kButtonHeight));
  button->SetTextColorId(views::LabelButton::ButtonState::STATE_NORMAL,
                         ui::kColorSysOnSurface);
  button->SetImageLabelSpacing(kButtonImageLabelSpacing);
  button->SetBorder(views::CreatePaddedBorder(
      views::CreateThemedRoundedRectBorder(kButtonBorderThickness,
                                           kButtonCornerRadius,
                                           ui::kColorSysTonalOutline),
      kButtonPadding));
  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kStart);
  AddChildView(std::move(button));

  auto textfield = std::make_unique<views::Textfield>();
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  textfield->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_MAHI_MENU_INPUT_TEXTHOLDER));
  textfield->SetProperty(views::kFlexBehaviorKey,
                         views::FlexSpecification(views::FlexSpecification(
                             views::MinimumFlexSizeRule::kPreferred,
                             views::MaximumFlexSizeRule::kUnbounded)));
  textfield->SetProperty(views::kMarginsKey,
                         gfx::Insets::TLBR(kButtonTextfieldSpacing, 0, 0, 0));
  AddChildView(std::move(textfield));
}

MahiMenuView::~MahiMenuView() = default;

void MahiMenuView::RequestFocus() {
  views::View::RequestFocus();

  // TODO(b/319735347): Add browsertest for this behavior.
  settings_button_->RequestFocus();
}

// static
views::UniqueWidgetPtr MahiMenuView::CreateWidget(
    const gfx::Rect& anchor_view_bounds) {
  views::Widget::InitParams params;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = kWidgetName;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  MahiMenuView* mahi_menu_view =
      widget->SetContentsView(std::make_unique<MahiMenuView>());
  mahi_menu_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void MahiMenuView::UpdateBounds(const gfx::Rect& anchor_view_bounds) {
  // TODO(b/318733414): Move `editor_menu::GetEditorMenuBounds` to a common
  // place for use
  GetWidget()->SetBounds(
      editor_menu::GetEditorMenuBounds(anchor_view_bounds, this));
}

void MahiMenuView::OnSummaryButtonPressed() {
  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  chromeos::MahiManager::Get()->OpenMahiPanel(display.id());
}

BEGIN_METADATA(MahiMenuView)
END_METADATA

}  // namespace chromeos::mahi
