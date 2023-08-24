// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"

#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

constexpr char kWidgetName[] = "EditorMenuPromoCardViewWidget";
constexpr char16_t kTitleTextPlaceholder[] =
    u"Editor menu title text placeholder";
constexpr char16_t kDescriptionTextPlaceholder[] =
    u"Editor menu description text placeholder";

constexpr int kPromoCardIconSizeDip = 48;

constexpr int kContainerPaddingDip = 16;
constexpr int kContainerMinWidthDip = 368;

// Spacing between this view and the anchor view (context menu).
constexpr int kMarginDip = 8;

}  // namespace

EditorMenuPromoCardView::EditorMenuPromoCardView(
    const gfx::Rect& anchor_view_bounds)
    : pre_target_handler_(
          std::make_unique<PreTargetHandler>(this, CardType::kEditorMenu)) {
  InitLayout();
}

EditorMenuPromoCardView::~EditorMenuPromoCardView() = default;

// static
views::UniqueWidgetPtr EditorMenuPromoCardView::CreateWidget(
    const gfx::Rect& anchor_view_bounds) {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = kWidgetName;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  EditorMenuPromoCardView* editor_menu_promo_card_view =
      widget->SetContentsView(
          std::make_unique<EditorMenuPromoCardView>(anchor_view_bounds));
  editor_menu_promo_card_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void EditorMenuPromoCardView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
}

void EditorMenuPromoCardView::RequestFocus() {
  views::View::RequestFocus();
  dismiss_button_->RequestFocus();
}

void EditorMenuPromoCardView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(kTitleTextPlaceholder);
}

void EditorMenuPromoCardView::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
}

void EditorMenuPromoCardView::OnWidgetActivationChanged(views::Widget* widget,
                                                        bool active) {
  // When the widget is active, use default focus behavior.
  if (active) {
    pre_target_handler_.reset();
    return;
  }

  // Close widget when it is deactivated.
  GetWidget()->Close();
}

void EditorMenuPromoCardView::UpdateBounds(
    const gfx::Rect& anchor_view_bounds) {
  const int height = GetHeightForWidth(anchor_view_bounds.width());
  int y = anchor_view_bounds.y() - kMarginDip - height;

  // The Editor Menu view will be off screen if showing above the anchor.
  // Show below the anchor instead.
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(anchor_view_bounds)
              .work_area()
              .y()) {
    y = anchor_view_bounds.bottom() + kMarginDip;
  }

  const gfx::Rect bounds = {{anchor_view_bounds.x(), y},
                            {kContainerMinWidthDip, height}};
  GetWidget()->SetBounds(bounds);
}

void EditorMenuPromoCardView::InitLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(kContainerPaddingDip));

  // Icon.
  auto* icon = AddChildView(std::make_unique<views::ImageView>());
  icon->SetImage(gfx::CreateVectorIcon(vector_icons::kGoogleColorIcon,
                                       kPromoCardIconSizeDip,
                                       gfx::kPlaceholderColor));

  // The main view, which shows the promo card text and buttons.
  auto* main_view = AddChildView(std::make_unique<views::FlexLayoutView>());
  main_view->SetOrientation(views::LayoutOrientation::kVertical);
  main_view->SetCollapseMargins(true);
  main_view->SetIgnoreDefaultMainAxisMargins(true);
  main_view->SetDefault(views::kMarginsKey,
                        gfx::Insets::VH(kContainerPaddingDip, 0));

  InitTextContainer(main_view);
  InitButtonBar(main_view);
}

void EditorMenuPromoCardView::InitTextContainer(views::View* main_view) {
  // Text container layout.
  auto* text_container =
      main_view->AddChildView(std::make_unique<views::FlexLayoutView>());
  text_container->SetOrientation(views::LayoutOrientation::kVertical);
  text_container->SetCollapseMargins(true);
  text_container->SetIgnoreDefaultMainAxisMargins(true);

  const int vertical_spacing = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  text_container->SetDefault(views::kMarginsKey,
                             gfx::Insets::VH(vertical_spacing, 0));

  // Title.
  auto* title = text_container->AddChildView(std::make_unique<views::Label>(
      kTitleTextPlaceholder, views::style::CONTEXT_DIALOG_TITLE));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetMultiLine(true);

  // Description.
  auto* description =
      text_container->AddChildView(std::make_unique<views::Label>(
          kDescriptionTextPlaceholder, views::style::CONTEXT_DIALOG_BODY_TEXT));
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description->SetMultiLine(true);
}

void EditorMenuPromoCardView::InitButtonBar(views::View* main_view) {
  // Button bar layout.
  auto* button_bar =
      main_view->AddChildView(std::make_unique<views::FlexLayoutView>());
  button_bar->SetOrientation(views::LayoutOrientation::kHorizontal);
  button_bar->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  button_bar->SetCollapseMargins(true);
  button_bar->SetIgnoreDefaultMainAxisMargins(true);

  const int button_spacing = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  button_bar->SetDefault(views::kMarginsKey,
                         gfx::Insets::VH(0, button_spacing));

  // Dismiss button.
  dismiss_button_ =
      button_bar->AddChildView(std::make_unique<views::LabelButton>(
          views::Button::PressedCallback(),
          l10n_util::GetStringUTF16(
              IDS_EDITOR_MENU_PROMO_CARD_VIEW_DISMISS_BUTTON)));

  // Tell me more button.
  button_bar->AddChildView(std::make_unique<views::LabelButton>(
      views::Button::PressedCallback(),
      l10n_util::GetStringUTF16(
          IDS_EDITOR_MENU_PROMO_CARD_VIEW_TELL_ME_MORE_BUTTON)));
}

BEGIN_METADATA(EditorMenuPromoCardView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
