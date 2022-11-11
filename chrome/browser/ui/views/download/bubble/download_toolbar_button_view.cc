// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"

#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"
#include "chrome/browser/ui/views/download/bubble/download_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/progress_ring_utils.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

constexpr int kProgressRingRadius = 9;
constexpr int kProgressRingRadiusTouchMode = 12;
constexpr float kProgressRingStrokeWidth = 1.7f;
// 7.5 rows * 60 px per row = 450;
constexpr int kMaxHeightForRowList = 450;
}  // namespace

DownloadToolbarButtonView::DownloadToolbarButtonView(BrowserView* browser_view)
    : ToolbarButton(
          base::BindRepeating(&DownloadToolbarButtonView::ButtonPressed,
                              base::Unretained(this))),
      browser_(browser_view->browser()) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetVectorIcons(kDownloadToolbarButtonIcon, kDownloadToolbarButtonIcon);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kDialog);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_DOWNLOAD_ICON));
  SetVisible(false);

  scanning_animation_.SetSlideDuration(base::Milliseconds(2500));
  scanning_animation_.SetTweenType(gfx::Tween::LINEAR);

  bubble_controller_ = std::make_unique<DownloadBubbleUIController>(browser_);
  // Wait until we're done with everything else before creating `controller_`
  // since it can call `Show()` synchronously.
  controller_ = std::make_unique<DownloadDisplayController>(
      this, browser_, bubble_controller_.get());
}

DownloadToolbarButtonView::~DownloadToolbarButtonView() {
  controller_.reset();
  bubble_controller_.reset();
}

void DownloadToolbarButtonView::PaintButtonContents(gfx::Canvas* canvas) {
  DownloadDisplayController::ProgressInfo progress_info =
      controller_->GetProgress();
  DownloadDisplayController::IconInfo icon_info = controller_->GetIconInfo();
  // Do not show the progress ring when there is no in progress download.
  if (progress_info.download_count == 0) {
    if (scanning_animation_.is_animating()) {
      scanning_animation_.End();
    }
    return;
  }

  bool is_disabled = GetVisualState() == Button::STATE_DISABLED;
  bool is_active = icon_info.is_active;
  SkColor background_color, progress_color;
  if (is_disabled) {
    background_color = GetForegroundColor(ButtonState::STATE_DISABLED);
    progress_color =
        icon_color_.value_or(GetForegroundColor(ButtonState::STATE_DISABLED));
  } else if (!is_active) {
    background_color =
        GetColorProvider()->GetColor(kColorDownloadToolbarButtonRingBackground);
    progress_color = icon_color_.value_or(
        GetColorProvider()->GetColor(kColorDownloadToolbarButtonInactive));
  } else {
    background_color =
        GetColorProvider()->GetColor(kColorDownloadToolbarButtonRingBackground);
    progress_color = icon_color_.value_or(
        GetColorProvider()->GetColor(kColorDownloadToolbarButtonActive));
  }

  int ring_radius = ui::TouchUiController::Get()->touch_ui()
                        ? kProgressRingRadiusTouchMode
                        : kProgressRingRadius;
  int x = width() / 2 - ring_radius;
  int y = height() / 2 - ring_radius;
  int diameter = 2 * ring_radius;
  gfx::RectF ring_bounds(x, y, /*width=*/diameter, /*height=*/diameter);

  if (icon_info.icon_state == download::DownloadIconState::kDeepScanning ||
      !progress_info.progress_certain) {
    if (!scanning_animation_.is_animating()) {
      scanning_animation_.Reset();
      scanning_animation_.Show();
    }
    views::DrawSpinningRing(canvas, gfx::RectFToSkRect(ring_bounds),
                            background_color, progress_color,
                            kProgressRingStrokeWidth, /*start_angle=*/
                            gfx::Tween::IntValueBetween(
                                scanning_animation_.GetCurrentValue(), 0, 360));
    return;
  }

  views::DrawProgressRing(
      canvas, gfx::RectFToSkRect(ring_bounds), background_color, progress_color,
      kProgressRingStrokeWidth, /*start_angle=*/-90,
      /*sweep_angle=*/360 * progress_info.progress_percentage / 100.0);
}

void DownloadToolbarButtonView::Show() {
  SetVisible(true);
  PreferredSizeChanged();
}

void DownloadToolbarButtonView::Hide() {
  HideDetails();
  SetVisible(false);
  PreferredSizeChanged();
}

bool DownloadToolbarButtonView::IsShowing() {
  return GetVisible();
}

void DownloadToolbarButtonView::Enable() {
  SetEnabled(true);
}

void DownloadToolbarButtonView::Disable() {
  SetEnabled(false);
}

void DownloadToolbarButtonView::UpdateDownloadIcon() {
  UpdateIcon();
}

bool DownloadToolbarButtonView::IsFullscreenWithParentViewHidden() {
  return browser_->window()->IsFullscreen() &&
         !browser_->window()->IsToolbarVisible();
}

// This function shows the partial view. If the main view is already showing,
// we do not show the partial view. If the partial view is already showing,
// there is nothing to do here, the controller should update the partial view.
void DownloadToolbarButtonView::ShowDetails() {
  if (!bubble_delegate_) {
    is_primary_partial_view_ = true;
    CreateBubbleDialogDelegate(GetPrimaryView());
  }
}

void DownloadToolbarButtonView::HideDetails() {
  CloseDialog(views::Widget::ClosedReason::kUnspecified);
}

bool DownloadToolbarButtonView::IsShowingDetails() {
  return bubble_delegate_ != nullptr;
}

void DownloadToolbarButtonView::UpdateIcon() {
  if (!GetWidget())
    return;

  // Schedule paint to update the progress ring.
  SchedulePaint();

  DownloadDisplayController::IconInfo icon_info = controller_->GetIconInfo();
  const gfx::VectorIcon* new_icon;
  SkColor icon_color = GetIconColor();
  bool is_touch_mode = ui::TouchUiController::Get()->touch_ui();
  if (icon_info.icon_state == download::DownloadIconState::kProgress ||
      icon_info.icon_state == download::DownloadIconState::kDeepScanning) {
    new_icon = is_touch_mode ? &kDownloadInProgressTouchIcon
                             : &kDownloadInProgressIcon;
  } else {
    new_icon = is_touch_mode ? &kDownloadToolbarButtonTouchIcon
                             : &kDownloadToolbarButtonIcon;
  }

  SetImageModel(ButtonState::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
  SetImageModel(ButtonState::STATE_HOVERED,
                ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
  SetImageModel(ButtonState::STATE_PRESSED,
                ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
  SetImageModel(
      Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(
          *new_icon, GetForegroundColor(ButtonState::STATE_DISABLED)));
}

std::unique_ptr<views::View> DownloadToolbarButtonView::GetPrimaryView() {
  if (is_primary_partial_view_) {
    return CreateRowListView(bubble_controller_->GetPartialView());
  } else {
    // raw ptr is safe as the toolbar view owns the bubble.
    return std::make_unique<DownloadDialogView>(
        browser_, CreateRowListView(bubble_controller_->GetMainView()), this);
  }
}

void DownloadToolbarButtonView::OpenPrimaryDialog() {
  primary_view_->SetVisible(true);
  security_view_->SetVisible(false);
  bubble_delegate_->SetButtons(ui::DIALOG_BUTTON_NONE);
  ResizeDialog();
}

void DownloadToolbarButtonView::OpenSecurityDialog(
    DownloadBubbleRowView* download_row_view) {
  security_view_->UpdateSecurityView(download_row_view);
  primary_view_->SetVisible(false);
  security_view_->SetVisible(true);
  security_view_->UpdateAccessibilityTextAndFocus();
  ResizeDialog();
}

void DownloadToolbarButtonView::CloseDialog(
    views::Widget::ClosedReason reason) {
  if (bubble_delegate_)
    bubble_delegate_->GetWidget()->CloseWithReason(reason);
}

void DownloadToolbarButtonView::ResizeDialog() {
  // Resize may be called when there is no delegate, e.g. during bubble
  // construction.
  if (bubble_delegate_)
    bubble_delegate_->SizeToContents();
}

void DownloadToolbarButtonView::OnBubbleDelegateDeleted() {
  bubble_delegate_ = nullptr;
  primary_view_ = nullptr;
  security_view_ = nullptr;
}

// TODO(crbug.com/1350148): Remove the margin around the bubble.
// Hover button should be visible from end to end of the bubble.
void DownloadToolbarButtonView::CreateBubbleDialogDelegate(
    std::unique_ptr<View> bubble_contents_view) {
  if (!bubble_contents_view)
    return;
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      this, views::BubbleBorder::TOP_RIGHT);
  bubble_delegate->SetTitle(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_HEADER_TEXT));
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetButtons(ui::DIALOG_BUTTON_NONE);
  bubble_delegate->RegisterDeleteDelegateCallback(
      base::BindOnce(&DownloadToolbarButtonView::OnBubbleDelegateDeleted,
                     weak_factory_.GetWeakPtr()));
  auto* switcher_view =
      bubble_delegate->SetContentsView(std::make_unique<views::View>());
  switcher_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  primary_view_ = switcher_view->AddChildView(std::move(bubble_contents_view));
  // raw ptr for this and member fields are safe as Toolbar Button view owns the
  // Bubble.
  security_view_ =
      switcher_view->AddChildView(std::make_unique<DownloadBubbleSecurityView>(
          bubble_controller_.get(), this, bubble_delegate.get()));
  security_view_->SetVisible(false);
  bubble_delegate->set_fixed_width(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble_delegate->set_margins(
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  bubble_delegate_ = bubble_delegate.get();
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
  bubble_delegate_->GetWidget()->Show();
}

// If the bubble delegate is set (either the main or the partial view), the
// button press is going to make the bubble lose focus, and will destroy
// the bubble.
// If the bubble delegate is not set, show the main view.
void DownloadToolbarButtonView::ButtonPressed() {
  if (!bubble_delegate_) {
    is_primary_partial_view_ = false;
    CreateBubbleDialogDelegate(GetPrimaryView());
  }
  controller_->OnButtonPressed();
}

void DownloadToolbarButtonView::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  UpdateIcon();
}

std::unique_ptr<views::View> DownloadToolbarButtonView::CreateRowListView(
    std::vector<DownloadUIModel::DownloadUIModelPtr> model_list) {
  // Do not create empty partial view.
  if (is_primary_partial_view_ && model_list.empty())
    return nullptr;

  auto row_list_view = std::make_unique<DownloadBubbleRowListView>(
      is_primary_partial_view_, browser_);
  for (DownloadUIModel::DownloadUIModelPtr& model : model_list) {
    // raw pointer is safe as the toolbar owns the bubble, which owns an
    // individual row view.
    row_list_view->AddChildView(std::make_unique<DownloadBubbleRowView>(
        std::move(model), row_list_view.get(), bubble_controller_.get(), this,
        browser_));
  }

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetContents(std::move(row_list_view));
  scroll_view->ClipHeightTo(0, kMaxHeightForRowList);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kEnabled);
  return std::move(scroll_view);
}

SkColor DownloadToolbarButtonView::GetIconColor() const {
  return icon_color_.value_or(
      controller_->GetIconInfo().is_active
          ? GetColorProvider()->GetColor(kColorDownloadToolbarButtonActive)
          : GetColorProvider()->GetColor(kColorDownloadToolbarButtonInactive));
}

void DownloadToolbarButtonView::SetIconColor(SkColor color) {
  if (icon_color_ == color)
    return;
  icon_color_ = color;
  UpdateIcon();
}

BEGIN_METADATA(DownloadToolbarButtonView, ToolbarButton)
END_METADATA
