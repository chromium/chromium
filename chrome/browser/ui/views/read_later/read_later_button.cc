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
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button.h"

ReadLaterButton::ReadLaterButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&ReadLaterButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser),
      webui_bubble_manager_(std::make_unique<WebUIBubbleManager<ReadLaterUI>>(
          this,
          browser->profile(),
          GURL(chrome::kChromeUIReadLaterURL))) {
  SetTooltipText(l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE));
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // We don't want to use ToolbarButton::SetHighlight here because it adds a
  // border around the button.
  LabelButton::SetText(l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE));
}

ReadLaterButton::~ReadLaterButton() = default;

const char* ReadLaterButton::GetClassName() const {
  return "ReadLaterButton";
}

void ReadLaterButton::UpdateIcon() {
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    kReadLaterIcon,
                    GetThemeProvider()->GetColor(
                        ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON),
                    GetIconSize()));
}

int ReadLaterButton::GetIconSize() const {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  return (touch_ui && !browser_->app_controller()) ? kDefaultTouchableIconSize
                                                   : kDefaultIconSize;
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
