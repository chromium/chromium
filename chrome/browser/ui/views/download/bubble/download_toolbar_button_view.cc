// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"

#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_controller.h"
#include "chrome/browser/ui/views/download/bubble/download_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/layout_provider.h"

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
  content::DownloadManager* manager = profile->GetDownloadManager();
  // The display starts hidden and isn't shown until a download is initiated.
  // TODO(anise): Use pref service to determine what the initial state
  // should be.
  SetVisible(false);
  controller_ = std::make_unique<DownloadDisplayController>(this, manager);
  bubble_controller_ = std::make_unique<DownloadBubbleUIController>(manager);
}

DownloadToolbarButtonView::~DownloadToolbarButtonView() {
  controller_.reset();
  bubble_controller_.reset();
}

void DownloadToolbarButtonView::Show() {
  SetVisible(true);
  ButtonPressed();
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

void DownloadToolbarButtonView::UpdateDownloadIcon(
    download::DownloadIconState state) {
  icon_state_ = state;
  UpdateIcon();
}

void DownloadToolbarButtonView::UpdateIcon() {
  if (!GetWidget())
    return;

  const gfx::VectorIcon* new_icon;
  SkColor icon_color;
  if (icon_state_ == download::DownloadIconState::kProgress) {
    icon_color = GetColorProvider()->GetColor(ui::kColorThrobber);
    new_icon = &kDownloadInProgressIcon;
  } else {
    icon_color = GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR);
    new_icon = &kDownloadToolbarButtonIcon;
  }

  if (icon_color != gfx::kPlaceholderColor) {
    for (auto state : kButtonStates) {
      SetImageModel(state,
                    ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
    }
  }

  // TODO(anise): Add progress ring animation.
}

void DownloadToolbarButtonView::OnBubbleDelegateDeleted() {
  bubble_delegate_ = nullptr;
}

std::unique_ptr<views::BubbleDialogDelegate>
DownloadToolbarButtonView::CreateBubbleDialogDelegate() {
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
  bubble_delegate->SetContentsView(std::make_unique<DownloadDialogView>(
      browser_, bubble_controller_->GetMainView()));

  bubble_delegate->set_fixed_width(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble_delegate->set_margins(
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  return bubble_delegate;
}

// We do not need to hide the bubble if it is already showing, as it will be
// destroyed because of loss of focus.
void DownloadToolbarButtonView::ButtonPressed() {
  if (!bubble_delegate_) {
    std::unique_ptr<views::BubbleDialogDelegate> bubble_delegate =
        CreateBubbleDialogDelegate();
    bubble_delegate_ = bubble_delegate.get();
    views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
    bubble_delegate_->GetWidget()->Show();
  }
}

BEGIN_METADATA(DownloadToolbarButtonView, ToolbarButton)
END_METADATA
