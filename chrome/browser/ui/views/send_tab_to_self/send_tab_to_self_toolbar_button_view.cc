// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_button_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button_controller.h"

namespace send_tab_to_self {

SendTabToSelfToolbarButtonView::SendTabToSelfToolbarButtonView(
    BrowserView* browser_view)
    : ToolbarButton(
          base::BindRepeating(&SendTabToSelfToolbarButtonView::ButtonPressed,
                              base::Unretained(this))),
      browser_(browser_view->browser()) {
  SetFlipCanvasOnPaintForRTLUI(false);
  SetVectorIcons(kSendTabToSelfIcon, kSendTabToSelfIcon);
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_BUTTON_A11Y_NAME));
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_TITLE));

  // We start hidden and only show once the controller tells us to.
  SetVisible(false);

  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->SetDelegate(this);
}

SendTabToSelfToolbarButtonView::~SendTabToSelfToolbarButtonView() {
  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->SetDelegate(nullptr);
}

void SendTabToSelfToolbarButtonView::Show(const SendTabToSelfEntry& entry) {
  entry_ = &entry;
  SetVisible(true);
}

void SendTabToSelfToolbarButtonView::ButtonPressed() {
  SendTabToSelfToolbarBubbleView::CreateBubble(
      browser_->profile(), this, *entry_, base::BindOnce(&Navigate));
}

void SendTabToSelfToolbarButtonView::DismissEntry(std::string& guid) {
  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->DismissEntries(std::vector<std::string>({guid}));
  SetVisible(false);
}

void SendTabToSelfToolbarButtonView::LogNotificationOpened() {
  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->LogNotificationOpened();
}

void SendTabToSelfToolbarButtonView::LogNotificationDismissed() {
  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->LogNotificationDismissed();
}

}  // namespace send_tab_to_self
