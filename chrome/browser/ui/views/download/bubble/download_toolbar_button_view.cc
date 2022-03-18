// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"

#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/progress_ring_utils.h"
#include "ui/views/layout/layout_provider.h"

namespace {

constexpr int kProgressRingRadius = 9;
constexpr float kProgressRingStrokeWidth = 1.7f;

std::unique_ptr<DownloadBubbleRowListView> CreateRowListView(
    std::vector<DownloadUIModelPtr> model_list,
    DownloadBubbleUIController* bubble_controller) {
  auto row_list_view = std::make_unique<DownloadBubbleRowListView>();
  for (DownloadUIModelPtr& model : model_list) {
    row_list_view->AddChildView(std::make_unique<DownloadBubbleRowView>(
        std::move(model), row_list_view.get(), bubble_controller));
  }
  return row_list_view;
}

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
  Profile* profile = browser_->profile();
  SetVisible(false);

  bubble_controller_ = std::make_unique<DownloadBubbleUIController>(profile);
  // Wait until we're done with everything else before creating `controller_`
  // since it can call `Show()` synchronously.
  controller_ = std::make_unique<DownloadDisplayController>(
      this, profile, bubble_controller_.get());
}

DownloadToolbarButtonView::~DownloadToolbarButtonView() {
  controller_.reset();
  bubble_controller_.reset();
}

void DownloadToolbarButtonView::PaintButtonContents(gfx::Canvas* canvas) {
  DownloadDisplayController::ProgressInfo progress_info =
      controller_->GetProgress();
  // Do not show the progress ring when there is no in progress download.
  if (progress_info.download_count == 0) {
    return;
  }

  int x = width() / 2 - kProgressRingRadius;
  int y = height() / 2 - kProgressRingRadius;
  int diameter = 2 * kProgressRingRadius;
  gfx::RectF ring_bounds(x, y, /*width=*/diameter, /*height=*/diameter);

  views::DrawProgressRing(
      canvas, gfx::RectFToSkRect(ring_bounds),
      GetColorProvider()->GetColor(kColorDownloadToolbarButtonRingBackground),
      GetColorProvider()->GetColor(kColorDownloadToolbarButtonActive),
      kProgressRingStrokeWidth, /*start_angle=*/-90,
      /*sweep_angle=*/360 * progress_info.progress_percentage / 100.0);
}

void DownloadToolbarButtonView::Show() {
  SetVisible(true);
  PreferredSizeChanged();
}

void DownloadToolbarButtonView::Hide() {
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

// This function shows the partial view. If the main view is already showing,
// we do not show the partial view. If the partial view is already showing,
// there is nothing to do here, the controller should update the partial view.
void DownloadToolbarButtonView::ShowDetails() {
  if (!bubble_delegate_) {
    std::unique_ptr<views::BubbleDialogDelegate> bubble_delegate =
        CreateBubbleDialogDelegate(CreateRowListView(
            bubble_controller_->GetPartialView(), bubble_controller_.get()));
    bubble_delegate_ = bubble_delegate.get();
    views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
    bubble_delegate_->GetWidget()->Show();
  }
}

void DownloadToolbarButtonView::UpdateIcon() {
  if (!GetWidget())
    return;

  // Schedule paint to update the progress ring.
  SchedulePaint();

  DownloadDisplayController::IconInfo icon_info = controller_->GetIconInfo();
  const gfx::VectorIcon* new_icon;
  SkColor icon_color =
      icon_info.is_active
          ? GetColorProvider()->GetColor(kColorDownloadToolbarButtonActive)
          : GetColorProvider()->GetColor(kColorDownloadToolbarButtonInactive);
  if (icon_info.icon_state == download::DownloadIconState::kProgress) {
    new_icon = &kDownloadInProgressIcon;
  } else {
    new_icon = &kDownloadToolbarButtonIcon;
  }

  if (icon_color != gfx::kPlaceholderColor) {
    for (auto state : kButtonStates) {
      SetImageModel(state,
                    ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
    }
  }
}

void DownloadToolbarButtonView::OnBubbleDelegateDeleted() {
  bubble_delegate_ = nullptr;
}

std::unique_ptr<views::BubbleDialogDelegate>
DownloadToolbarButtonView::CreateBubbleDialogDelegate(
    std::unique_ptr<View> bubble_contents_view) {
  std::unique_ptr<views::BubbleDialogDelegate> bubble_delegate =
      std::make_unique<views::BubbleDialogDelegate>(
          this, views::BubbleBorder::TOP_RIGHT);
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetButtons(ui::DIALOG_BUTTON_NONE);
  // base::Unretained(this) is fine as DownloadToolbarButtonView is the anchor
  // view, and owns the child view/widgets.
  bubble_delegate->RegisterDeleteDelegateCallback(
      base::BindOnce(&DownloadToolbarButtonView::OnBubbleDelegateDeleted,
                     base::Unretained(this)));
  bubble_delegate->SetContentsView(std::move(bubble_contents_view));

  bubble_delegate->set_fixed_width(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble_delegate->set_margins(
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  return bubble_delegate;
}

// If the bubble delegate is set (either the main or the partial view), the
// button press is going to make the bubble lose focus, and will destroy
// the bubble.
// If the bubble delegate is not set, show the main view.
void DownloadToolbarButtonView::ButtonPressed() {
  if (!bubble_delegate_) {
    std::unique_ptr<views::BubbleDialogDelegate> bubble_delegate =
        CreateBubbleDialogDelegate(std::make_unique<DownloadDialogView>(
            browser_, CreateRowListView(bubble_controller_->GetMainView(),
                                        bubble_controller_.get())));
    bubble_delegate_ = bubble_delegate.get();
    views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
    bubble_delegate_->GetWidget()->Show();
  }
  controller_->OnButtonPressed();
}

BEGIN_METADATA(DownloadToolbarButtonView, ToolbarButton)
END_METADATA
