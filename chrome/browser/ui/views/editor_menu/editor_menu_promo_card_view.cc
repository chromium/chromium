// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
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
    u"Editor menu description text placeholder text to introduce the editor "
    u"menu feature";

constexpr int kPromoCardIconSizeDip = 48;

// Spacing around the main containers in the promo card. This is applied between
// the icon container, text container and promo card edges.
constexpr int kPromoCardHorizontalPaddingDip = 16;
constexpr gfx::Insets kPromoCardInsets =
    gfx::Insets::VH(12, kPromoCardHorizontalPaddingDip);

int GetPromoCardLabelWidth(int promo_card_width) {
  return promo_card_width - kPromoCardInsets.width() - kPromoCardIconSizeDip -
         kPromoCardHorizontalPaddingDip;
}

}  // namespace

EditorMenuPromoCardView::EditorMenuPromoCardView(
    const gfx::Rect& anchor_view_bounds,
    EditorMenuViewDelegate* delegate)
    : pre_target_handler_(
          std::make_unique<PreTargetHandler>(this, CardType::kEditorMenu)),
      delegate_(delegate) {
  CHECK(delegate_);
  InitLayout();
}

EditorMenuPromoCardView::~EditorMenuPromoCardView() = default;

// static
views::UniqueWidgetPtr EditorMenuPromoCardView::CreateWidget(
    const gfx::Rect& anchor_view_bounds,
    EditorMenuViewDelegate* delegate) {
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
  EditorMenuPromoCardView* editor_menu_promo_card_view =
      widget->SetContentsView(std::make_unique<EditorMenuPromoCardView>(
          anchor_view_bounds, delegate));
  editor_menu_promo_card_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void EditorMenuPromoCardView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
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

  CHECK(delegate_);
  delegate_->OnPromoCardWidgetClosed(widget->closed_reason());
}

void EditorMenuPromoCardView::OnWidgetActivationChanged(views::Widget* widget,
                                                        bool active) {
  // When the widget is active, use default focus behavior.
  if (active) {
    // The widget could be activated by an event handled by the pre target
    // handler. Reset the pre target handler asynchronously to avoid destroying
    // it while it is still handling this event.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&EditorMenuPromoCardView::ResetPreTargetHandler,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // Close widget when it is deactivated.
  GetWidget()->Close();
}

bool EditorMenuPromoCardView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  CHECK(GetWidget());
  GetWidget()->Close();
  return true;
}

void EditorMenuPromoCardView::UpdateBounds(
    const gfx::Rect& anchor_view_bounds) {
  const int promo_card_width = GetEditorMenuWidth(anchor_view_bounds.width());

  // Multiline labels aren't resized properly in flex or box layouts. According
  // to https://crbug.com/678337#c7 this isn't easy to fix. As a workaround, we
  // explicitly set the labels' maximum width.
  const int label_width = GetPromoCardLabelWidth(promo_card_width);
  title_->SetMaximumWidth(label_width);
  description_->SetMaximumWidth(label_width);

  // Since the width of the labels can affect their preferred height, compute
  // promo card preferred size after label width has been set.
  const gfx::Size promo_card_size(promo_card_width,
                                  GetHeightForWidth(promo_card_width));

  GetWidget()->SetBounds(
      GetEditorMenuBounds(anchor_view_bounds, promo_card_size));
}

void EditorMenuPromoCardView::InitLayout() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysSurface, views::LayoutProvider::Get()->GetCornerRadiusMetric(
                                views::ShapeContextTokens::kMenuRadius)));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, kPromoCardInsets);

  // Icon.
  auto* icon = AddChildView(std::make_unique<views::ImageView>());
  icon->SetImage(gfx::CreateVectorIcon(vector_icons::kGoogleColorIcon,
                                       kPromoCardIconSizeDip,
                                       gfx::kPlaceholderColor));

  // The main view, which shows the promo card text and buttons.
  auto* main_view = AddChildView(std::make_unique<views::FlexLayoutView>());
  main_view->SetOrientation(views::LayoutOrientation::kVertical);
  main_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  AddTitle(main_view);
  AddDescription(main_view);
  AddButtonBar(main_view);
}

void EditorMenuPromoCardView::AddTitle(views::View* main_view) {
  title_ = main_view->AddChildView(std::make_unique<views::Label>(
      kTitleTextPlaceholder, views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_HEADLINE_5));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetMultiLine(true);
  title_->SetEnabledColorId(ui::kColorSysOnSurface);
}

void EditorMenuPromoCardView::AddDescription(views::View* main_view) {
  description_ = main_view->AddChildView(std::make_unique<views::Label>(
      kDescriptionTextPlaceholder, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_BODY_3));
  description_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_->SetMultiLine(true);
  description_->SetEnabledColorId(ui::kColorSysOnSurfaceSubtle);
  description_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(views::LayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT),
                        0,
                        views::LayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT),
                        0));
}

void EditorMenuPromoCardView::AddButtonBar(views::View* main_view) {
  // Button bar layout.
  auto* button_bar =
      main_view->AddChildView(std::make_unique<views::FlexLayoutView>());
  button_bar->SetOrientation(views::LayoutOrientation::kHorizontal);
  button_bar->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  button_bar->SetCollapseMargins(true);
  button_bar->SetIgnoreDefaultMainAxisMargins(true);
  button_bar->SetDefault(
      views::kMarginsKey,
      gfx::Insets::VH(0, views::LayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  // Dismiss button.
  dismiss_button_ =
      button_bar->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&EditorMenuPromoCardView::CloseWidgetWithReason,
                              weak_factory_.GetWeakPtr(),
                              views::Widget::ClosedReason::kCloseButtonClicked),
          l10n_util::GetStringUTF16(
              IDS_EDITOR_MENU_PROMO_CARD_VIEW_DISMISS_BUTTON)));
  dismiss_button_->SetStyle(ui::ButtonStyle::kText);

  // Tell me more button.
  tell_me_more_button_ =
      button_bar->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &EditorMenuPromoCardView::CloseWidgetWithReason,
              weak_factory_.GetWeakPtr(),
              views::Widget::ClosedReason::kAcceptButtonClicked),
          l10n_util::GetStringUTF16(
              IDS_EDITOR_MENU_PROMO_CARD_VIEW_TELL_ME_MORE_BUTTON)));
  tell_me_more_button_->SetStyle(ui::ButtonStyle::kProminent);
}

void EditorMenuPromoCardView::CloseWidgetWithReason(
    views::Widget::ClosedReason closed_reason) {
  CHECK(GetWidget());
  GetWidget()->CloseWithReason(closed_reason);
}

void EditorMenuPromoCardView::ResetPreTargetHandler() {
  pre_target_handler_.reset();
}

BEGIN_METADATA(EditorMenuPromoCardView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
