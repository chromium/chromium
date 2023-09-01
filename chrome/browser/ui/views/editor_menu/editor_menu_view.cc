// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"

#include <array>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_chip_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_textfield_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

constexpr char kWidgetName[] = "EditorMenuViewWidget";
constexpr char16_t kContainerTitle[] = u"Editor Menu";

constexpr int kContainerMinWidthDip = 368;
constexpr int kRadiusDip = 4;

constexpr gfx::Insets kTitleContainerInsets = gfx::Insets::TLBR(10, 16, 10, 10);

constexpr char16_t kBadgeText[] = u"New";
constexpr gfx::Insets kBadgeInsets = gfx::Insets::VH(0, 8);
constexpr int kBadgeHorizontalPaddingDip = 8;
constexpr int kBadgeVerticalPaddingDip = 8;
constexpr SkColor kBadgeBackgroundColorStart = SkColorSetRGB(0xB5, 0xC4, 0xFF);
constexpr SkColor kBadgeBackgroundColorEnd = SkColorSetRGB(0xB3, 0xEF, 0xD4);

constexpr char16_t kSettingsTooltipString[] = u"Settings";
constexpr int kSettingsIconSizeDip = 20;
constexpr int kSettingsButtonBorderDip = 4;

constexpr int kChipsContainerVerticalSpacingDip = 16;
constexpr gfx::Insets kChipsMargin =
    gfx::Insets::TLBR(0, 8, kChipsContainerVerticalSpacingDip, 0);
constexpr gfx::Insets kChipsContainerInsets = gfx::Insets::TLBR(0, 8, 0, 8);

constexpr gfx::Insets kTextfieldContainerInsets =
    gfx::Insets::TLBR(0, 16, 10, 16);

// Spacing between this view and the anchor view (context menu).
constexpr int kMarginDip = 8;

// TODO(b/295059934): Call EditorMediator API to get the actual labels.
constexpr std::array<std::u16string_view, 6> kChipLables = {
    u"chip label 1", u"chip label 2", u"chip label 3",
    u"chip label 4", u"chip label 5", u"chip label 6",
};

// A background that fills the canvas with rounded corners and gradient colors.
class GradientRoundedRectBackground : public views::Background {
 public:
  GradientRoundedRectBackground(float radius,
                                SkColor start_color,
                                SkColor end_color)
      : radii_(gfx::RoundedCornersF{radius}),
        start_color_(start_color),
        end_color_(end_color) {}

  GradientRoundedRectBackground(const GradientRoundedRectBackground&) = delete;
  GradientRoundedRectBackground& operator=(
      const GradientRoundedRectBackground&) = delete;

  ~GradientRoundedRectBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    const auto& bounds = view->GetContentsBounds();
    gfx::Rect rect(bounds);
    SkPath path;
    SkScalar radii[8] = {radii_.upper_left(),  radii_.upper_left(),
                         radii_.upper_right(), radii_.upper_right(),
                         radii_.lower_right(), radii_.lower_right(),
                         radii_.lower_left(),  radii_.lower_left()};
    path.addRoundRect(gfx::RectToSkRect(rect), radii);

    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrcOver);
    flags.setShader(gfx::CreateGradientShader(
        bounds.left_center(), bounds.right_center(), start_color_, end_color_));
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(path, flags);
  }

 private:
  const gfx::RoundedCornersF radii_;
  const SkColor start_color_;
  const SkColor end_color_;
};

}  // namespace

EditorMenuView::EditorMenuView(const gfx::Rect& anchor_view_bounds,
                               EditorMenuViewDelegate* delegate)
    : pre_target_handler_(
          std::make_unique<PreTargetHandler>(this, CardType::kEditorMenu)),
      delegate_(delegate) {
  CHECK(delegate_);
  InitLayout();
}

EditorMenuView::~EditorMenuView() = default;

// static
views::UniqueWidgetPtr EditorMenuView::CreateWidget(
    const gfx::Rect& anchor_view_bounds,
    EditorMenuViewDelegate* delegate) {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = kWidgetName;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  EditorMenuView* editor_menu_view = widget->SetContentsView(
      std::make_unique<EditorMenuView>(anchor_view_bounds, delegate));
  editor_menu_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void EditorMenuView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

void EditorMenuView::RequestFocus() {
  views::View::RequestFocus();
  settings_button_->RequestFocus();
}

void EditorMenuView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(kContainerTitle);
}

bool EditorMenuView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  GetWidget()->Close();
  return true;
}

void EditorMenuView::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
}

void EditorMenuView::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
  // When the widget is active, will use default focus behavior.
  if (active) {
    // Reset `pre_target_handler_` immediately causes problems if the events are
    // not all precessed. Reset it asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&EditorMenuView::ResetPreTargetHandler,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  // Close widget When it is deactivated.
  GetWidget()->Close();
}

void EditorMenuView::UpdateBounds(const gfx::Rect& anchor_view_bounds) {
  int height = GetHeightForWidth(anchor_view_bounds.width());
  int y = anchor_view_bounds.y() - kMarginDip - height;

  // The Editor Menu view will be off screen if showing above the anchor.
  // Show below the anchor instead.
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(anchor_view_bounds)
              .work_area()
              .y()) {
    y = anchor_view_bounds.bottom() + kMarginDip;
  }

  gfx::Rect bounds = {{anchor_view_bounds.x(), y},
                      {kContainerMinWidthDip, height}};
  GetWidget()->SetBounds(bounds);
}

void EditorMenuView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysAppBase, kRadiusDip));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  AddTitleContainer();
  AddChipsContainer();
  AddTextfield();
}

void EditorMenuView::AddTitleContainer() {
  title_container_ = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* layout =
      title_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* title = title_container_->AddChildView(std::make_unique<views::Label>(
      kContainerTitle, views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_HEADLINE_5));
  title->SetEnabledColorId(ui::kColorSysOnSurface);

  auto* badge =
      title_container_->AddChildView(std::make_unique<views::FlexLayoutView>());
  badge->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  badge->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  badge->SetProperty(views::kMarginsKey, kBadgeInsets);
  auto* text = badge->AddChildView(std::make_unique<views::Label>(
      kBadgeText, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_2));
  text->SetEnabledColorId(ui::kColorSysOnSurface);
  badge->SetPreferredSize(gfx::Size(
      text->GetPreferredSize().width() + 2 * kBadgeHorizontalPaddingDip,
      text->GetPreferredSize().height() + 2 * kBadgeVerticalPaddingDip));
  float radius = badge->GetPreferredSize().height() / 2.0f;
  badge->SetBackground(std::make_unique<GradientRoundedRectBackground>(
      radius, kBadgeBackgroundColorStart, kBadgeBackgroundColorEnd));

  auto* spacer =
      title_container_->AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(spacer, 1);

  auto* button_container =
      title_container_->AddChildView(std::make_unique<views::FlexLayoutView>());
  button_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  button_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  settings_button_ =
      button_container->AddChildView(std::make_unique<views::ImageButton>(
          base::BindRepeating(&EditorMenuView::OnSettingsButtonPressed,
                              weak_factory_.GetWeakPtr())));
  settings_button_->SetTooltipText(kSettingsTooltipString);
  settings_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsOutlineIcon,
                                     cros_tokens::kCrosSysOnSurface,
                                     kSettingsIconSizeDip));
  settings_button_->SetBorder(
      views::CreateEmptyBorder(kSettingsButtonBorderDip));
  views::InkDrop::Get(settings_button_)
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(settings_button_)->SetBaseColorId(ui::kColorIcon);
  settings_button_->SetHasInkDropActionOnClick(true);

  title_container_->SetProperty(views::kMarginsKey, kTitleContainerInsets);

  int width = kContainerMinWidthDip - kTitleContainerInsets.width();
  int height = std::max(title->GetPreferredSize().height(),
                        settings_button_->GetPreferredSize().height());
  title_container_->SetPreferredSize(gfx::Size(width, height));
}

void EditorMenuView::AddChipsContainer() {
  chips_container_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  chips_container_->SetOrientation(views::LayoutOrientation::kVertical);

  // Add a new row if the chip cannot fit the rest space in the current row.
  // A simple calculation of the running width, considering the margins and
  // paddings.
  int running_width = 0;
  views::View* row = nullptr;
  int index = 0;
  for (const auto& label : kChipLables) {
    auto chip = std::make_unique<EditorMenuChipView>(
        base::BindRepeating(&EditorMenuView::OnChipButtonPressed,
                            weak_factory_.GetWeakPtr(), index++),
        label.data(), &vector_icons::kKeyboardIcon);

    int chip_width = chip->GetPreferredSize().width();
    if (running_width == 0) {
      // Add the container's left insets.
      running_width += kChipsContainerInsets.left();
      running_width += chip_width;
    } else {
      // Add the chip's left margin.
      running_width += kChipsMargin.left();
      running_width += chip_width;
    }
    // Add the containers's right insets when decide if wrap the row.
    const bool should_wrap_row =
        running_width + kChipsContainerInsets.right() > kContainerMinWidthDip;
    if (row == nullptr || should_wrap_row) {
      running_width = should_wrap_row ? 0 : running_width;
      row = chips_container_->AddChildView(std::make_unique<views::View>());
      auto* layout =
          row->SetLayoutManager(std::make_unique<views::FlexLayout>());
      layout->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetInteriorMargin(kChipsContainerInsets)
          .SetDefault(views::kMarginsKey, kChipsMargin);
    }
    chips_.emplace_back(row->AddChildView(std::move(chip)));
  }
}

void EditorMenuView::AddTextfield() {
  textfield_ =
      AddChildView(std::make_unique<EditorMenuTextfieldView>(delegate_));
  textfield_->SetProperty(views::kMarginsKey, kTextfieldContainerInsets);

  int width = kContainerMinWidthDip - kTextfieldContainerInsets.width();
  int height = textfield_->GetHeightForWidth(width);
  textfield_->SetPreferredSize(gfx::Size(width, height));
}

void EditorMenuView::OnSettingsButtonPressed() {
  CHECK(delegate_);
  delegate_->OnSettingsButtonPressed();
}

void EditorMenuView::OnChipButtonPressed(int button_id) {
  CHECK(delegate_);
  delegate_->OnChipButtonPressed(button_id, textfield_->textfield()->GetText());
}

void EditorMenuView::ResetPreTargetHandler() {
  pre_target_handler_.reset();
}

BEGIN_METADATA(EditorMenuView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
