// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_button_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/vector_icons/vector_icons.h"
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
  // TODO(crbug/1206381): SetTooltipText(...);

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

void SendTabToSelfToolbarButtonView::Show() {
  SetVisible(true);
}

void SendTabToSelfToolbarButtonView::Hide() {
  SetVisible(false);
}

void SendTabToSelfToolbarButtonView::ButtonPressed() {
  NOTIMPLEMENTED();
}

}  // namespace send_tab_to_self
