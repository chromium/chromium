// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/read_later/read_later_button.h"

#include "base/strings/string16.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"

ReadLaterButton::ReadLaterButton(Browser* browser)
    : LabelButton(base::BindRepeating(&ReadLaterButton::ButtonPressed,
                                      base::Unretained(this)),
                  l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE)),
      browser_(browser),
      webui_bubble_manager_(std::make_unique<WebUIBubbleManager<ReadLaterUI>>(
          IDS_READ_LATER_TITLE,
          this,
          browser->profile(),
          GURL(chrome::kChromeUIReadLaterURL))) {
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));

  views::InstallPillHighlightPathGenerator(this);
  SetInkDropMode(InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetInkDropVisibleOpacity(kToolbarInkDropVisibleOpacity);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE));
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);

  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
}

ReadLaterButton::~ReadLaterButton() = default;

const char* ReadLaterButton::GetClassName() const {
  return "ReadLaterButton";
}

std::unique_ptr<views::InkDrop> ReadLaterButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnFocus(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropHighlight>
ReadLaterButton::CreateInkDropHighlight() const {
  return CreateToolbarInkDropHighlight(this);
}

SkColor ReadLaterButton::GetInkDropBaseColor() const {
  return GetToolbarInkDropBaseColor(this);
}

void ReadLaterButton::OnThemeChanged() {
  // We don't always have a theme provider (ui tests, for example).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;
  const SkColor color =
      theme_provider->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
  SetEnabledTextColors(color);
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          kReadLaterIcon, color_utils::DeriveDefaultIconColor(color)));

  LabelButton::OnThemeChanged();
}

void ReadLaterButton::ButtonPressed() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);

  if (browser_view->side_panel()) {
    if (!read_later_side_panel_bubble_) {
      browser_view->side_panel()->RemoveContent(read_later_side_panel_bubble_);
      read_later_side_panel_bubble_ = nullptr;
      // TODO(pbos): Observe read_later_side_panel_bubble_ so we don't need to
      // SetHighlighted(false) here.
      SetHighlighted(false);
    } else {
      auto web_view = std::make_unique<WebUIBubbleView>(browser_->profile());
      web_view->LoadURL<ReadLaterUI>(GURL(chrome::kChromeUIReadLaterURL));
      auto bubble_view =
          std::make_unique<WebUIBubbleDialogView>(this, std::move(web_view));
      read_later_side_panel_bubble_ = bubble_view.get();
      browser_view->side_panel()->AddContent(std::move(bubble_view));
      SetHighlighted(true);
    }
  } else {
    if (webui_bubble_manager_->GetBubbleWidget()) {
      webui_bubble_manager_->CloseBubble();
    } else {
      webui_bubble_manager_->ShowBubble();
    }
  }
}
