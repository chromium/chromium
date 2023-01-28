// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_pre_target_handler.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

// Buttons view.
constexpr int kButtonsViewMarginDip = 4;
constexpr int kButtonsSpacingDip = 4;
constexpr int kSettingsButtonSizeDip = 14;
constexpr int kSettingsButtonBorderDip = 3;

}  // namespace

// RichAnswersView -----------------------------------------------------------

RichAnswersView::RichAnswersView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller)
    : anchor_view_bounds_(anchor_view_bounds),
      controller_(std::move(controller)),
      focus_search_(std::make_unique<QuickAnswersFocusSearch>(
          this,
          base::BindRepeating(&RichAnswersView::GetFocusableViews,
                              base::Unretained(this)))) {
  InitLayout();
  InitWidget();

  // Focus.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
}

RichAnswersView::~RichAnswersView() = default;

const char* RichAnswersView::GetClassName() const {
  return "RichAnswersView";
}

void RichAnswersView::OnFocus() {
  View* wants_focus = focus_search_->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kCheckStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kSkipAnchoredDialog, nullptr,
      nullptr);
  if (wants_focus != this) {
    wants_focus->RequestFocus();
  } else {
    NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  }
}

void RichAnswersView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(ui::kColorPrimaryBackground)));
  if (settings_button_) {
    settings_button_->SetImage(
        views::Button::ButtonState::STATE_NORMAL,
        gfx::CreateVectorIcon(
            vector_icons::kSettingsOutlineIcon, kSettingsButtonSizeDip,
            GetColorProvider()->GetColor(ui::kColorIconSecondary)));
  }
}

views::FocusTraversable* RichAnswersView::GetPaneFocusTraversable() {
  return focus_search_.get();
}

void RichAnswersView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;

  node_data->SetName(
      l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT));
}

void RichAnswersView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Base view Layout.
  base_view_ = AddChildView(std::make_unique<View>());
  auto* base_layout =
      base_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  base_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  // TODO(b/259440976): Add child views for each result type.

  // Add util buttons in the top-right corner.
  AddFrameButtons();
}

void RichAnswersView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);
  UpdateBounds();
}

void RichAnswersView::AddFrameButtons() {
  auto* buttons_view = AddChildView(std::make_unique<views::View>());
  auto* layout =
      buttons_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(gfx::Insets(kButtonsViewMarginDip))
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, kButtonsSpacingDip, 0, 0));

  settings_button_ = buttons_view->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &QuickAnswersUiController::OnSettingsButtonPressed, controller_)));
  settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_QUICK_ANSWERS_SETTINGS_BUTTON_TOOLTIP_TEXT));
  settings_button_->SetBorder(
      views::CreateEmptyBorder(kSettingsButtonBorderDip));
}

void RichAnswersView::UpdateBounds() {
  auto display_bounds = display::Screen::GetScreen()
                            ->GetDisplayMatching(anchor_view_bounds_)
                            .bounds();

  // TODO(b/259440976): Calculate desired bounds based on anchor view bounds.
  gfx::Rect bounds = {
      {display_bounds.width() / 2 - 200, display_bounds.height() / 2 - 300},
      {400, 600}};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Ash, convert the position relative to the screen.
  // For Lacros, `bounds` is already relative to the toplevel window and the
  // position will be calculated on server side.
  wm::ConvertRectFromScreen(GetWidget()->GetNativeWindow()->parent(), &bounds);
#endif
  GetWidget()->SetBounds(bounds);
}

std::vector<views::View*> RichAnswersView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  focusable_views.push_back(this);

  if (settings_button_ && settings_button_->GetVisible()) {
    focusable_views.push_back(settings_button_);
  }

  return focusable_views;
}
