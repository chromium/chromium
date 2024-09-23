// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_definition_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_translation_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_unit_conversion_view.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "url/gurl.h"

namespace {

using quick_answers::QuickAnswer;
using quick_answers::QuickAnswerResultText;
using quick_answers::ResultType;

// Rich card dimensions.
constexpr int kDefaultRichCardWidth = 360;
constexpr int kMinimumRichCardHeight = 120;
constexpr int kMaximumRichCardHeight = 464;

// View dimensions.
constexpr auto kMainViewInsets = gfx::Insets::TLBR(20, 20, 16, 20);

// Buttons view.
constexpr int kSettingsButtonSizeDip = 20;

// Border corner radius.
constexpr int kRoundedCornerRadius = 12;

// Google search link.
constexpr auto kSearchLinkViewInsets = gfx::Insets::TLBR(0, 60, 20, 20);

}  // namespace

namespace quick_answers {

// RichAnswersView -----------------------------------------------------------

RichAnswersView::RichAnswersView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    const ResultType result_type)
    : anchor_view_bounds_(anchor_view_bounds),
      controller_(std::move(controller)),
      result_type_(result_type) {
  InitLayout();

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF8(IDS_RICH_ANSWERS_VIEW_A11Y_NAME_TEXT));
}

RichAnswersView::~RichAnswersView() = default;

views::UniqueWidgetPtr RichAnswersView::CreateWidget(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    const QuickAnswer& quick_answer,
    const StructuredResult& result) {
  // Create the correct rich card child view depending on the result type.
  std::unique_ptr<RichAnswersView> child_view = nullptr;
  switch (quick_answer.result_type) {
    case ResultType::kDefinitionResult: {
      child_view = std::make_unique<RichAnswersDefinitionView>(
          anchor_view_bounds, controller, *result.definition_result.get());
      break;
    }
    case ResultType::kTranslationResult: {
      child_view = std::make_unique<RichAnswersTranslationView>(
          anchor_view_bounds, controller, *result.translation_result.get());
      break;
    }
    case ResultType::kUnitConversionResult: {
      child_view = std::make_unique<RichAnswersUnitConversionView>(
          anchor_view_bounds, controller, *result.unit_conversion_result.get());
      break;
    }
    case ResultType::kNoResult: {
      return views::UniqueWidgetPtr();
    }
  }

  CHECK(child_view);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.corner_radius = kRoundedCornerRadius;
  params.name = kWidgetName;
  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));

  RichAnswersView* rich_answers_view =
      widget->SetContentsView(std::move(child_view));
  rich_answers_view->UpdateBounds();
  rich_answers_view->SetPaintToLayer();
  rich_answers_view->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kRoundedCornerRadius));
  rich_answers_view->layer()->SetIsFastRoundedCorner(true);

  return widget;
}

void RichAnswersView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
}

void RichAnswersView::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
}

void RichAnswersView::OnKeyEvent(ui::KeyEvent* event) {
  // TODO(b/283135347): Track rich card interaction types for metrics.
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  if (event->key_code() == ui::VKEY_ESCAPE) {
    QuickAnswersController::Get()->DismissQuickAnswers(
        quick_answers::QuickAnswersExitPoint::kUnspecified);
  }
}

void RichAnswersView::OnThemeChanged() {
  views::View::OnThemeChanged();

  search_link_label_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorSysPrimary));
}

void RichAnswersView::OnWidgetActivationChanged(views::Widget* widget,
                                                bool active) {
  // TODO(b/283135347): Track rich card interaction types for metrics.
  if (!active && widget->IsVisible()) {
    QuickAnswersController::Get()->DismissQuickAnswers(
        quick_answers::QuickAnswersExitPoint::kUnspecified);
  }
}

ui::ImageModel RichAnswersView::GetIconImageModelForTesting() {
  return vector_icon_ ? vector_icon_->GetImageModel() : ui::ImageModel();
}

void RichAnswersView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Set up the scrollable base view that contains all the rich card components.
  SetUpBaseView();

  // Set up the main view that contains the icon and content view.
  SetUpMainView();

  // Add icon that corresponds to the quick answer result type.
  AddResultTypeIcon();

  // Set up the content view that will be populated by the rich card subclasses.
  SetUpContentView();

  // Add google search link label at the bottom of the base view.
  AddGoogleSearchLink();
}

void RichAnswersView::SetUpBaseView() {
  views::ScrollView* scroll_view = AddChildView(
      views::Builder<views::ScrollView>()
          .ClipHeightTo(kMinimumRichCardHeight, kMaximumRichCardHeight)
          .SetBackgroundThemeColorId(ui::kColorPrimaryBackground)
          .SetHorizontalScrollBarMode(
              views::ScrollView::ScrollBarMode::kDisabled)
          .SetDrawOverflowIndicator(false)
          .SetAllowKeyboardScrolling(true)
          .Build());

  base_view_ = scroll_view->SetContents(std::make_unique<views::View>());
  auto* base_layout =
      base_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  base_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
}

void RichAnswersView::SetUpMainView() {
  // This box layout will have the view flex values as:
  // - result type icon (flex=0): no resize
  // - content_view_ (flex=1): resize (either shrink or expand as necessary)
  main_view_ = base_view_->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetInsideBorderInsets(kMainViewInsets)
          .SetBetweenChildSpacing(kContentDoubleSpacing)
          .Build());
}

void RichAnswersView::SetUpContentView() {
  content_view_ = main_view_->AddChildView(CreateVerticalBoxLayoutView());
  content_view_->SetMinimumCrossAxisSize(kContentTextWidth);

  main_view_->SetFlexForView(content_view_, /*flex=*/1);
}

void RichAnswersView::AddResultTypeIcon() {
  // Add the icon representing the quick answers result type as well as
  // a circle background behind the icon.
  auto* vector_icon_container =
      main_view_->AddChildView(std::make_unique<views::FlexLayoutView>());
  vector_icon_container->SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysPrimary, kRichAnswersIconContainerRadius));
  vector_icon_container->SetBorder(
      views::CreateEmptyBorder(kRichAnswersIconBorderDip));

  vector_icon_ =
      vector_icon_container->AddChildView(std::make_unique<views::ImageView>());
  vector_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      GetResultTypeIcon(result_type_), ui::kColorSysBaseContainerElevated,
      /*icon_size=*/kRichAnswersIconSizeDip));

  main_view_->SetFlexForView(vector_icon_container, /*flex=*/0);
}

views::View* RichAnswersView::AddSettingsButtonTo(views::View* container_view) {
  CHECK(container_view);

  settings_button_ = container_view->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &QuickAnswersUiController::OnSettingsButtonPressed, controller_)));
  settings_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsOutlineIcon,
                                     ui::kColorSysOnSurface,
                                     /*icon_size=*/kSettingsButtonSizeDip));
  settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_RICH_ANSWERS_VIEW_SETTINGS_BUTTON_A11Y_NAME_TEXT));

  return settings_button_;
}

void RichAnswersView::AddHeaderViewsTo(views::View* container_view,
                                       const std::string& header_text) {
  // This box layout will have the view flex values as:
  // - header_label (flex=1): resize (either shrink or expand as necessary)
  // - settings_button_view (flex=0): no resize
  views::BoxLayoutView* box_layout_view =
      container_view->AddChildView(CreateHorizontalBoxLayoutView());
  box_layout_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* header_label =
      box_layout_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
          header_text, GetFontList(TypographyToken::kCrosButton2),
          kContentHeaderWidth,
          /*is_multi_line=*/false, ui::kColorSysSecondary));

  views::View* settings_button_view = AddSettingsButtonTo(box_layout_view);

  box_layout_view->SetFlexForView(header_label, /*flex=*/1);
  box_layout_view->SetFlexForView(settings_button_view, /*flex=*/0);
}

void RichAnswersView::AddGoogleSearchLink() {
  auto* search_link_view = base_view_->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetInteriorMargin(kSearchLinkViewInsets)
          .Build());

  search_link_label_ = search_link_view->AddChildView(
      std::make_unique<views::Link>(l10n_util::GetStringUTF16(
          IDS_RICH_ANSWERS_VIEW_SEARCH_LINK_LABEL_TEXT)));
  search_link_label_->SetCallback(base::BindRepeating(
      &RichAnswersView::OnGoogleSearchLinkClicked, weak_factory_.GetWeakPtr()));
  search_link_label_->SetFontList(GetFontList(TypographyToken::kCrosButton2));
  search_link_label_->SetForceUnderline(false);
}

void RichAnswersView::OnGoogleSearchLinkClicked() {
  CHECK(controller_);
  controller_->OnGoogleSearchLabelPressed();
}

void RichAnswersView::UpdateBounds() {
  auto display_bounds = display::Screen::GetScreen()
                            ->GetDisplayMatching(anchor_view_bounds_)
                            .work_area();
  int preferred_height = GetPreferredSize().height();
  gfx::Rect bounds = {
      {anchor_view_bounds_.x(), anchor_view_bounds_.y() - preferred_height / 2},
      {kDefaultRichCardWidth, preferred_height}};
  bounds.AdjustToFit(display_bounds);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Ash, convert the position relative to the screen.
  // For Lacros, `bounds` is already relative to the top-level window and
  // the position will be calculated on server side.
  wm::ConvertRectFromScreen(GetWidget()->GetNativeWindow()->parent(), &bounds);
#endif

  GetWidget()->SetBounds(bounds);
}

views::View* RichAnswersView::GetContentView() {
  CHECK(content_view_);

  return content_view_;
}

BEGIN_METADATA(RichAnswersView)
END_METADATA

}  // namespace quick_answers
